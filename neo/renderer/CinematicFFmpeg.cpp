/*
===========================================================================
FFmpeg-backed cinematic (videomap). Optional: define ID_HAVE_FFMPEG and
link libavformat, libavcodec, libavutil, libswscale (and swresample if
your FFmpeg build requires it). See docs/FFMPEG_VIDEO.md and neo/_FFmpeg.props.
===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "CinematicFFmpeg.h"

#ifdef ID_HAVE_FFMPEG

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

struct idCinematicFFmpeg::Pimpl {
	struct MemBuf {
		const uint8_t *data;
		int size;
		int pos;
	};

	AVFormatContext *	fmt;
	AVIOContext *		avio;
	uint8_t *			avioBuffer;
	MemBuf				ioMem;
	AVCodecContext *	dec;
	const AVCodec *		codec;
	SwsContext *		sws;
	AVFrame *			frame;
	AVFrame *			frameRgb;
	AVPacket *			pkt;
	uint8_t *			fileData;
	int					fileLen;
	int					videoStream;
	int					width;
	int					height;
	int					animationMs;
	bool				looping;
	int					lastWallMs;
	int64_t				lastFramePts;
	byte *				rgba;
	bool				eof;

	Pimpl()
		: fmt( NULL ), avio( NULL ), avioBuffer( NULL ), dec( NULL ), codec( NULL ), sws( NULL ),
		  frame( NULL ), frameRgb( NULL ), pkt( NULL ), fileData( NULL ), fileLen( 0 ),
		  videoStream( -1 ), width( 0 ), height( 0 ), animationMs( 0 ), looping( false ),
		  lastWallMs( -1 ), lastFramePts( AV_NOPTS_VALUE ), rgba( NULL ), eof( false ) {
		ioMem.data = NULL;
		ioMem.size = 0;
		ioMem.pos = 0;
	}

	~Pimpl() {
		Cleanup();
	}

	static int ReadMem( void *opaque, uint8_t *buf, int buf_size ) {
		MemBuf *b = (MemBuf *)opaque;
		const int rem = b->size - b->pos;
		if ( rem <= 0 ) {
			return AVERROR_EOF;
		}
		const int n = buf_size < rem ? buf_size : rem;
		memcpy( buf, b->data + b->pos, n );
		b->pos += n;
		return n;
	}

	static int64_t SeekMem( void *opaque, int64_t offset, int whence ) {
		MemBuf *b = (MemBuf *)opaque;
		switch ( whence ) {
		case SEEK_SET:
			b->pos = (int)offset;
			break;
		case SEEK_CUR:
			b->pos += (int)offset;
			break;
		case SEEK_END:
			b->pos = b->size + (int)offset;
			break;
		case AVSEEK_SIZE:
			return b->size;
		default:
			return AVERROR( EINVAL );
		}
		if ( b->pos < 0 ) {
			b->pos = 0;
		}
		if ( b->pos > b->size ) {
			b->pos = b->size;
		}
		return b->pos;
	}

	void Cleanup() {
		if ( sws ) {
			sws_freeContext( sws );
			sws = NULL;
		}
		if ( frameRgb ) {
			av_frame_free( &frameRgb );
		}
		if ( frame ) {
			av_frame_free( &frame );
		}
		if ( pkt ) {
			av_packet_free( &pkt );
		}
		if ( dec ) {
			avcodec_free_context( &dec );
		}
		if ( fmt ) {
			avformat_close_input( &fmt );
			avio = NULL;
			avioBuffer = NULL;
		} else if ( avio ) {
			avio_context_free( &avio );
			avioBuffer = NULL;
		}
		if ( rgba ) {
			Mem_Free( rgba );
			rgba = NULL;
		}
		if ( fileData ) {
			fileSystem->FreeFile( fileData );
			fileData = NULL;
			fileLen = 0;
		}
		ioMem.data = NULL;
		ioMem.size = ioMem.pos = 0;
		videoStream = -1;
		lastWallMs = -1;
		lastFramePts = AV_NOPTS_VALUE;
		eof = false;
	}

	bool OpenDecoder() {
		const AVCodecParameters *par = fmt->streams[videoStream]->codecpar;
		codec = avcodec_find_decoder( par->codec_id );
		if ( !codec ) {
			return false;
		}
		dec = avcodec_alloc_context3( codec );
		if ( !dec ) {
			return false;
		}
		if ( avcodec_parameters_to_context( dec, par ) < 0 ) {
			return false;
		}
		dec->thread_count = 4;
		if ( avcodec_open2( dec, codec, NULL ) < 0 ) {
			return false;
		}
		width = dec->width;
		height = dec->height;
		if ( width < 1 || height < 1 ) {
			return false;
		}
		const int bufSize = av_image_get_buffer_size( AV_PIX_FMT_RGBA, width, height, 1 );
		if ( bufSize < 1 ) {
			return false;
		}
		frame = av_frame_alloc();
		frameRgb = av_frame_alloc();
		pkt = av_packet_alloc();
		if ( !frame || !frameRgb || !pkt ) {
			return false;
		}
		rgba = (byte *)Mem_Alloc( bufSize );
		if ( !rgba ) {
			return false;
		}
		av_image_fill_arrays( frameRgb->data, frameRgb->linesize, rgba, AV_PIX_FMT_RGBA, width, height, 1 );
		frameRgb->format = AV_PIX_FMT_RGBA;
		frameRgb->width = width;
		frameRgb->height = height;
		sws = sws_getContext( width, height, dec->pix_fmt, width, height, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL );
		return sws != NULL;
	}

	bool DecodeAndScaleOne() {
		while ( true ) {
			int err = av_read_frame( fmt, pkt );
			if ( err < 0 ) {
				return false;
			}
			if ( pkt->stream_index != videoStream ) {
				av_packet_unref( pkt );
				continue;
			}
			err = avcodec_send_packet( dec, pkt );
			av_packet_unref( pkt );
			if ( err < 0 && err != AVERROR( EAGAIN ) ) {
				return false;
			}
			err = avcodec_receive_frame( dec, frame );
			if ( err == 0 ) {
				sws_scale( sws, frame->data, frame->linesize, 0, height, frameRgb->data, frameRgb->linesize );
				lastFramePts = av_frame_get_best_effort_timestamp( frame );
				return true;
			}
			if ( err != AVERROR( EAGAIN ) ) {
				return false;
			}
		}
	}

	bool SeekToWallMs( int wallMs ) {
		if ( !fmt || videoStream < 0 || !dec ) {
			return false;
		}
		AVStream *st = fmt->streams[videoStream];
		const int64_t ts = av_rescale_q( (int64_t)wallMs, (AVRational){ 1, 1000 }, st->time_base );
		if ( av_seek_frame( fmt, videoStream, ts, AVSEEK_FLAG_BACKWARD ) < 0 ) {
			return false;
		}
		avcodec_flush_buffers( dec );
		lastFramePts = AV_NOPTS_VALUE;
		eof = false;
		return true;
	}

	bool EnsureFrame( int wallMs ) {
		if ( !fmt || !dec || !rgba ) {
			return false;
		}
		if ( wallMs < 0 ) {
			wallMs = 0;
		}
		const int len = animationMs;
		if ( len > 0 && wallMs >= len ) {
			if ( looping ) {
				wallMs = wallMs % len;
			} else {
				wallMs = len - 1;
			}
		}

		AVStream *st = fmt->streams[videoStream];
		const int64_t wantPts = av_rescale_q( (int64_t)wallMs, (AVRational){ 1, 1000 }, st->time_base );

		const bool backward = lastWallMs >= 0 && wallMs + 33 < lastWallMs;
		if ( backward || lastWallMs < 0 ) {
			if ( !SeekToWallMs( wallMs ) ) {
				return false;
			}
		}

		int guard = 0;
		while ( guard++ < 1024 ) {
			if ( lastFramePts != AV_NOPTS_VALUE ) {
				if ( av_compare_ts( lastFramePts, st->time_base, wantPts, st->time_base ) >= 0 ) {
					lastWallMs = wallMs;
					return true;
				}
			}
			if ( !DecodeAndScaleOne() ) {
				if ( looping && len > 0 ) {
					if ( !SeekToWallMs( wallMs % len ) ) {
						return false;
					}
					continue;
				}
				eof = true;
				return lastFramePts != AV_NOPTS_VALUE;
			}
		}
		return false;
	}
};

idCinematicFFmpeg::idCinematicFFmpeg() {
	p = new Pimpl();
}

idCinematicFFmpeg::~idCinematicFFmpeg() {
	Close();
	delete p;
	p = NULL;
}

bool idCinematicFFmpeg::InitFromFile( const char *qpath, bool looping ) {
	Close();

	idStr fileName;
	if ( strstr( qpath, "/" ) == NULL && strstr( qpath, "\\" ) == NULL ) {
		fileName = idStr( "video/" ) + qpath;
	} else {
		fileName = qpath;
	}

	p->looping = looping;
	p->lastWallMs = -1;
	p->lastFramePts = AV_NOPTS_VALUE;
	p->eof = false;

	const int n = fileSystem->ReadFile( fileName.c_str(), (void **)&p->fileData, NULL );
	if ( n <= 0 || !p->fileData ) {
		common->Warning( "idCinematicFFmpeg: could not read %s\n", fileName.c_str() );
		Close();
		return false;
	}
	p->fileLen = n;

	static bool loggedInit = false;
	if ( !loggedInit ) {
		av_log_set_level( AV_LOG_ERROR );
		loggedInit = true;
	}

	p->ioMem.data = p->fileData;
	p->ioMem.size = p->fileLen;
	p->ioMem.pos = 0;

	const int avioBufSize = 64 * 1024;
	p->avioBuffer = (uint8_t *)av_malloc( avioBufSize );
	if ( !p->avioBuffer ) {
		Close();
		return false;
	}
	p->avio = avio_alloc_context( p->avioBuffer, avioBufSize, 0, &p->ioMem, Pimpl::ReadMem, NULL, Pimpl::SeekMem );
	if ( !p->avio ) {
		av_freep( &p->avioBuffer );
		Close();
		return false;
	}

	p->fmt = avformat_alloc_context();
	if ( !p->fmt ) {
		Close();
		return false;
	}
	p->fmt->pb = p->avio;
	p->fmt->flags |= AVFMT_FLAG_CUSTOM_IO;

	if ( avformat_open_input( &p->fmt, "", NULL, NULL ) < 0 ) {
		common->Warning( "idCinematicFFmpeg: avformat_open_input failed for %s\n", fileName.c_str() );
		Close();
		return false;
	}

	if ( avformat_find_stream_info( p->fmt, NULL ) < 0 ) {
		common->Warning( "idCinematicFFmpeg: avformat_find_stream_info failed for %s\n", fileName.c_str() );
		Close();
		return false;
	}

	p->videoStream = av_find_best_stream( p->fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0 );
	if ( p->videoStream < 0 ) {
		common->Warning( "idCinematicFFmpeg: no video stream in %s\n", fileName.c_str() );
		Close();
		return false;
	}

	if ( !p->OpenDecoder() ) {
		common->Warning( "idCinematicFFmpeg: decoder init failed for %s\n", fileName.c_str() );
		Close();
		return false;
	}

	if ( p->fmt->duration != AV_NOPTS_VALUE && p->fmt->duration > 0 ) {
		p->animationMs = (int)( p->fmt->duration / 1000 );
	} else {
		p->animationMs = 0;
	}

	if ( !p->EnsureFrame( 0 ) ) {
		common->Warning( "idCinematicFFmpeg: could not decode first frame of %s\n", fileName.c_str() );
		Close();
		return false;
	}

	return true;
}

int idCinematicFFmpeg::AnimationLength() {
	if ( !p || p->animationMs <= 0 ) {
		return 0;
	}
	return p->animationMs;
}

cinData_t idCinematicFFmpeg::ImageForTime( int milliseconds ) {
	cinData_t out;
	memset( &out, 0, sizeof( out ) );
	if ( !p || !p->rgba || p->videoStream < 0 ) {
		return out;
	}

	p->EnsureFrame( milliseconds );

	out.imageWidth = p->width;
	out.imageHeight = p->height;
	out.image = p->rgba;
	out.status = p->eof ? FMV_EOF : FMV_PLAY;
	return out;
}

void idCinematicFFmpeg::Close() {
	if ( p ) {
		p->Cleanup();
	}
}

void idCinematicFFmpeg::ResetTime( int time ) {
	if ( p ) {
		p->lastWallMs = -1;
		p->lastFramePts = AV_NOPTS_VALUE;
		p->eof = false;
	}
}

#else /* !ID_HAVE_FFMPEG */

struct idCinematicFFmpeg::Pimpl {};

idCinematicFFmpeg::idCinematicFFmpeg() {
	p = NULL;
}

idCinematicFFmpeg::~idCinematicFFmpeg() {
}

bool idCinematicFFmpeg::InitFromFile( const char *qpath, bool looping ) {
	return false;
}

int idCinematicFFmpeg::AnimationLength() {
	return 0;
}

cinData_t idCinematicFFmpeg::ImageForTime( int milliseconds ) {
	cinData_t c;
	memset( &c, 0, sizeof( c ) );
	return c;
}

void idCinematicFFmpeg::Close() {
}

void idCinematicFFmpeg::ResetTime( int time ) {
}

#endif /* ID_HAVE_FFMPEG */
