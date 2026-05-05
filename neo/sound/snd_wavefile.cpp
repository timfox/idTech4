/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "snd_local.h"

#define DR_FLAC_NO_STDIO
#define DR_MP3_NO_STDIO
#include "third_party/dr_flac.h"
#include "third_party/dr_mp3.h"

namespace {

static void ResampleInterleavedS16( const short *src, int srcFrames, int srcRate, int channels, int dstRate,
	short **dstOut, int *dstFramesOut ) {
	if ( srcFrames <= 0 || channels < 1 || channels > 2 ) {
		*dstOut = NULL;
		*dstFramesOut = 0;
		return;
	}
	if ( srcRate == dstRate ) {
		const int n = srcFrames * channels;
		short *copy = (short *)Mem_Alloc( n * sizeof( short ) );
		memcpy( copy, src, n * sizeof( short ) );
		*dstOut = copy;
		*dstFramesOut = srcFrames;
		return;
	}
	const double ratio = (double)dstRate / (double)srcRate;
	const int dstFrames = ( int )idMath::Ceil( ( float )( ( double )srcFrames * ratio ) );
	const int dstSamples = dstFrames * channels;
	short *dst = (short *)Mem_Alloc( dstSamples * sizeof( short ) );
	for ( int f = 0; f < dstFrames; ++f ) {
		const double srcPos = ( ( double )f ) / ratio;
		const int i0 = ( int )idMath::Floor( ( float )srcPos );
		const int i1 = Min( i0 + 1, srcFrames - 1 );
		const float t = ( float )( srcPos - ( double )i0 );
		for ( int c = 0; c < channels; ++c ) {
			const float s0 = ( float )src[i0 * channels + c];
			const float s1 = ( float )src[i1 * channels + c];
			const float v = s0 * ( 1.0f - t ) + s1 * t;
			dst[f * channels + c] = ( short )idMath::ClampInt( -32768, 32767, ( int )idMath::Floor( v + 0.5f ) );
		}
	}
	*dstOut = dst;
	*dstFramesOut = dstFrames;
}

static int PickTargetSampleRate( int srcRate ) {
	if ( srcRate == 44100 || srcRate == 22050 || srcRate == 11025 ) {
		return srcRate;
	}
	return 44100;
}

} // namespace

/*
=============
idWaveFile::OpenFLACorMP3
Decode entire FLAC or MP3 into memory as 16-bit PCM (engine-supported rates).
=============
*/
int idWaveFile::OpenFLACorMP3( const char *strFileName, waveformatex_t *pwfx, bool flac ) {
	byte *buf = nullptr;
	const int len = fileSystem->ReadFile( strFileName, (void **)&buf, &mfileTime );
	if ( len <= 0 || !buf ) {
		return -1;
	}

	unsigned int ch = 0, rate = 0;
	drflac_uint64 totalFLAC = 0;
	drmp3_uint64 totalMP3 = 0;
	short *pcm = nullptr;

	if ( flac ) {
		pcm = drflac_open_memory_and_read_pcm_frames_s16( buf, (size_t)len, &ch, &rate, &totalFLAC, NULL );
	} else {
		pcm = drmp3_open_memory_and_read_pcm_frames_s16( buf, (size_t)len, &ch, &rate, &totalMP3, NULL );
	}
	fileSystem->FreeFile( buf );
	buf = nullptr;

	if ( !pcm || ch < 1 || ch > 2 || rate == 0 ) {
		if ( pcm ) {
			if ( flac ) {
				drflac_free( pcm, NULL );
			} else {
				drmp3_free( pcm, NULL );
			}
		}
		return -1;
	}

	const drflac_uint64 totalFrames = flac ? totalFLAC : totalMP3;
	if ( totalFrames == 0 ) {
		if ( flac ) {
			drflac_free( pcm, NULL );
		} else {
			drmp3_free( pcm, NULL );
		}
		return -1;
	}

	const int srcFrames = ( int )totalFrames;
	const int targetRate = PickTargetSampleRate( ( int )rate );
	short *resampled = nullptr;
	int outFrames = 0;
	ResampleInterleavedS16( pcm, srcFrames, ( int )rate, ( int )ch, targetRate, &resampled, &outFrames );
	if ( flac ) {
		drflac_free( pcm, NULL );
	} else {
		drmp3_free( pcm, NULL );
	}
	pcm = nullptr;

	if ( !resampled || outFrames <= 0 ) {
		return -1;
	}

	memset( &mpwfx, 0, sizeof( waveformatextensible_t ) );
	mpwfx.Format.wFormatTag = WAVE_FORMAT_TAG_PCM;
	mpwfx.Format.nChannels = ( word )ch;
	mpwfx.Format.nSamplesPerSec = ( dword )targetRate;
	mpwfx.Format.wBitsPerSample = 16;
	mpwfx.Format.nBlockAlign = ( word )( ch * sizeof( short ) );
	mpwfx.Format.nAvgBytesPerSec = targetRate * mpwfx.Format.nBlockAlign;

	mdwSize = ( dword )( outFrames * ch );
	mMemSize = mdwSize * sizeof( short );
	mpbData = resampled;
	mpbDataCur = mpbData;
	mulDataSize = mMemSize;
	mbIsReadingFromMemory = true;
	mhmmio = NULL;
	ogg = NULL;
	isOgg = false;

	if ( pwfx ) {
		memcpy( pwfx, &mpwfx.Format, sizeof( waveformatex_t ) );
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Name: idWaveFile::idWaveFile()
// Desc: Constructs the class.  Call Open() to open a wave file for reading.  
//       Then call Read() as needed.  Calling the destructor or Close() 
//       will close the file.  
//-----------------------------------------------------------------------------
idWaveFile::idWaveFile( void ) {
	memset( &mpwfx, 0, sizeof( waveformatextensible_t ) );
	mhmmio		= NULL;
	mdwSize		= 0;
	mseekBase	= 0;
	mbIsReadingFromMemory = false;
	mpbData		= NULL;
	ogg			= NULL;
	isOgg		= false;
}

//-----------------------------------------------------------------------------
// Name: idWaveFile::~idWaveFile()
// Desc: Destructs the class
//-----------------------------------------------------------------------------
idWaveFile::~idWaveFile( void ) {
	Close();

	if ( mbIsReadingFromMemory && mpbData ) {
		Mem_Free( mpbData );
	}

	memset( &mpwfx, 0, sizeof( waveformatextensible_t ) );
}

//-----------------------------------------------------------------------------
// Name: idWaveFile::Open()
// Desc: Opens a wave file for reading
//-----------------------------------------------------------------------------
int idWaveFile::Open( const char* strFileName, waveformatex_t* pwfx ) {

	mbIsReadingFromMemory = false;

	mpbData     = NULL;
	mpbDataCur  = mpbData;

	if( strFileName == NULL ) {
		return -1;
	}

	idStr ext;
	idStr( strFileName ).ExtractFileExtension( ext );
	ext.ToLower();
	if ( ext == "flac" ) {
		return OpenFLACorMP3( strFileName, pwfx, true );
	}
	if ( ext == "mp3" ) {
		return OpenFLACorMP3( strFileName, pwfx, false );
	}

	idStr name = strFileName;

	// note: used to only check for .wav when making a build
	name.SetFileExtension( ".ogg" );
	if ( fileSystem->ReadFile( name, NULL, NULL ) != -1 ) {
		return OpenOGG( name, pwfx );
	}

	idStr base = strFileName;
	base.StripFileExtension();
	idStr flacName = base;
	flacName.SetFileExtension( ".flac" );
	if ( fileSystem->ReadFile( flacName, NULL, NULL ) != -1 ) {
		return OpenFLACorMP3( flacName.c_str(), pwfx, true );
	}
	idStr mp3Name = base;
	mp3Name.SetFileExtension( ".mp3" );
	if ( fileSystem->ReadFile( mp3Name, NULL, NULL ) != -1 ) {
		return OpenFLACorMP3( mp3Name.c_str(), pwfx, false );
	}

	memset( &mpwfx, 0, sizeof( waveformatextensible_t ) );

	mhmmio = fileSystem->OpenFileRead( strFileName );
	if ( !mhmmio ) {
		mdwSize = 0;
		return -1;
	}
	if ( mhmmio->Length() <= 0 ) {
		mhmmio = NULL;
		return -1;
	}
	if ( ReadMMIO() != 0 ) {
		// ReadMMIO will fail if its an not a wave file
		Close();
		return -1;
	}

	mfileTime = mhmmio->Timestamp();

	if ( ResetFile() != 0 ) {
		Close();
		return -1;
	}

	// After the reset, the size of the wav file is mck.cksize so store it now
	mdwSize = mck.cksize / sizeof( short );
	mMemSize = mck.cksize;

	if ( mck.cksize != 0xffffffff ) {
		if ( pwfx ) {
			memcpy( pwfx, (waveformatex_t *)&mpwfx, sizeof(waveformatex_t));
		}
		return 0;
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Name: idWaveFile::OpenFromMemory()
// Desc: copy data to idWaveFile member variable from memory
//-----------------------------------------------------------------------------
int idWaveFile::OpenFromMemory( short* pbData, int ulDataSize, waveformatextensible_t* pwfx ) {
	mpwfx       = *pwfx;
	mulDataSize = ulDataSize;
	mpbData     = pbData;
	mpbDataCur  = mpbData;
	mdwSize		= ulDataSize / sizeof( short );
	mMemSize	= ulDataSize;
	mbIsReadingFromMemory = true;

	return 0;
}

//-----------------------------------------------------------------------------
// Name: idWaveFile::ReadMMIO()
// Desc: Support function for reading from a multimedia I/O stream.
//       mhmmio must be valid before calling.  This function uses it to
//       update mckRiff, and mpwfx. 
//-----------------------------------------------------------------------------
int idWaveFile::ReadMMIO( void ) {
	mminfo_t		ckIn;           // chunk info. for general use.
	pcmwaveformat_t pcmWaveFormat;  // Temp PCM structure to load in.       

	memset( &mpwfx, 0, sizeof( waveformatextensible_t ) );

	mhmmio->Read( &mckRiff, 12 );
	assert( !isOgg );
	mckRiff.ckid = LittleLong( mckRiff.ckid );
	mckRiff.cksize = LittleLong( mckRiff.cksize );
	mckRiff.fccType = LittleLong( mckRiff.fccType );
	mckRiff.dwDataOffset = 12;

	// Check to make sure this is a valid wave file
	if( (mckRiff.ckid != fourcc_riff) || (mckRiff.fccType != mmioFOURCC('W', 'A', 'V', 'E') ) ) {
		return -1;
	}

	// Search the input file for for the 'fmt ' chunk.
	ckIn.dwDataOffset = 12;
	do {
		if (8 != mhmmio->Read( &ckIn, 8 ) ) {
			return -1;
		}
		assert( !isOgg );
		ckIn.ckid = LittleLong( ckIn.ckid );
		ckIn.cksize = LittleLong( ckIn.cksize );
		ckIn.dwDataOffset += ckIn.cksize-8;
	} while (ckIn.ckid != mmioFOURCC('f', 'm', 't', ' '));

	// Expect the 'fmt' chunk to be at least as large as <PCMWAVEFORMAT>;
	// if there are extra parameters at the end, we'll ignore them
	if( ckIn.cksize < sizeof(pcmwaveformat_t) ) {
		return -1;
	}

	// Read the 'fmt ' chunk into <pcmWaveFormat>.
	if( mhmmio->Read( &pcmWaveFormat, sizeof(pcmWaveFormat) ) != sizeof(pcmWaveFormat) ) {
		return -1;
	}
	assert( !isOgg );
	pcmWaveFormat.wf.wFormatTag = LittleShort( pcmWaveFormat.wf.wFormatTag );
	pcmWaveFormat.wf.nChannels = LittleShort( pcmWaveFormat.wf.nChannels );
	pcmWaveFormat.wf.nSamplesPerSec = LittleLong( pcmWaveFormat.wf.nSamplesPerSec );
	pcmWaveFormat.wf.nAvgBytesPerSec = LittleLong( pcmWaveFormat.wf.nAvgBytesPerSec );
	pcmWaveFormat.wf.nBlockAlign = LittleShort( pcmWaveFormat.wf.nBlockAlign );
	pcmWaveFormat.wBitsPerSample = LittleShort( pcmWaveFormat.wBitsPerSample );

	// Copy the bytes from the pcm structure to the waveformatex_t structure
	memcpy( &mpwfx, &pcmWaveFormat, sizeof(pcmWaveFormat) );

	// Allocate the waveformatex_t, but if its not pcm format, read the next
	// word, and thats how many extra bytes to allocate.
	if( pcmWaveFormat.wf.wFormatTag == WAVE_FORMAT_TAG_PCM ) {
		mpwfx.Format.cbSize = 0;
	} else {
		return -1;	// we don't handle these (32 bit wavefiles, etc)
#if 0
		// Read in length of extra bytes.
		word cbExtraBytes = 0L;
		if( mhmmio->Read( (char*)&cbExtraBytes, sizeof(word) ) != sizeof(word) )
			return -1;

		mpwfx.Format.cbSize = cbExtraBytes;

		// Now, read those extra bytes into the structure, if cbExtraAlloc != 0.
		if( mhmmio->Read( (char*)(((byte*)&(mpwfx.Format.cbSize))+sizeof(word)), cbExtraBytes ) != cbExtraBytes ) {
			memset( &mpwfx, 0, sizeof( waveformatextensible_t ) );
			return -1;
		}
#endif
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Name: idWaveFile::ResetFile()
// Desc: Resets the internal mck pointer so reading starts from the 
//       beginning of the file again 
//-----------------------------------------------------------------------------
int idWaveFile::ResetFile( void ) {
	if( mbIsReadingFromMemory ) {
		mpbDataCur = mpbData;
	} else  {
		if( mhmmio == NULL ) {
			return -1;
		}

		// Seek to the data
		if( -1 == mhmmio->Seek( mckRiff.dwDataOffset + sizeof(fourcc), FS_SEEK_SET ) ) {
			return -1;
		}

		// Search the input file for for the 'fmt ' chunk.
		mck.ckid = 0;
		do {
			byte ioin;
			if ( !mhmmio->Read( &ioin, 1 ) ) {
				return -1;
			}
			mck.ckid = (mck.ckid>>8) | (ioin<<24);
		} while (mck.ckid != mmioFOURCC('d', 'a', 't', 'a'));
		
		mhmmio->Read( &mck.cksize, 4 );
		assert( !isOgg );
		mck.cksize = LittleLong( mck.cksize );
		mseekBase = mhmmio->Tell();
	}
	
	return 0;
}

//-----------------------------------------------------------------------------
// Name: idWaveFile::Read()
// Desc: Reads section of data from a wave file into pBuffer and returns 
//       how much read in pdwSizeRead, reading not more than dwSizeToRead.
//       This uses mck to determine where to start reading from.  So 
//       subsequent calls will be continue where the last left off unless 
//       Reset() is called.
//-----------------------------------------------------------------------------
int idWaveFile::Read( byte* pBuffer, int dwSizeToRead, int *pdwSizeRead ) {

	if ( ogg != NULL ) {

		return ReadOGG( pBuffer, dwSizeToRead, pdwSizeRead );

	} else if ( mbIsReadingFromMemory ) {

		if( mpbDataCur == NULL ) {
			return -1;
		}
		if( (byte*)(mpbDataCur + dwSizeToRead) > (byte*)(mpbData + mulDataSize) ) {
			dwSizeToRead = mulDataSize - (int)(mpbDataCur - mpbData);
		}
 		SIMDProcessor->Memcpy( pBuffer, mpbDataCur, dwSizeToRead );
		mpbDataCur += dwSizeToRead;

		if ( pdwSizeRead != NULL ) {
			*pdwSizeRead = dwSizeToRead;
		}

		return dwSizeToRead;

	} else {

		if( mhmmio == NULL ) {
			return -1;
		}
		if( pBuffer == NULL ) {
			return -1;
		}
		
		dwSizeToRead = mhmmio->Read( pBuffer, dwSizeToRead );
		// this is hit by ogg code, which does it's own byte swapping internally
		if ( !isOgg ) {
			LittleRevBytes( pBuffer, 2, dwSizeToRead / 2 );
		}

		if ( pdwSizeRead != NULL ) {
			*pdwSizeRead = dwSizeToRead;
		}

		return dwSizeToRead;
	}
}

//-----------------------------------------------------------------------------
// Name: idWaveFile::Close()
// Desc: Closes the wave file 
//-----------------------------------------------------------------------------
int idWaveFile::Close( void ) {
	if ( ogg != NULL ) {
		return CloseOGG();
	}
	if( mhmmio != NULL ) {
		fileSystem->CloseFile( mhmmio );
		mhmmio = NULL;
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Name: idWaveFile::Seek()
//-----------------------------------------------------------------------------
int idWaveFile::Seek( int offset ) {

	if ( ogg != NULL ) {

		common->FatalError( "idWaveFile::Seek: cannot seek on an OGG file\n" );

	} else if( mbIsReadingFromMemory ) {

		mpbDataCur = mpbData + offset;

	} else {
		if( mhmmio == NULL ) {
			return -1;
		}

		if ((int)(offset+mseekBase) == mhmmio->Tell()) {
			return 0;
		}
		mhmmio->Seek( offset + mseekBase, FS_SEEK_SET );
		return 0;
	}
	return -1;
}
