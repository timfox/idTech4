/*
===========================================================================

GlTF 2.0 (.gltf JSON or .glb) — triangle meshes with CPU morph targets.

Morph weights (per renderEntity, 0..1):
  shaderParms[ SHADERPARM_GLTF_MORPH0 .. SHADERPARM_GLTF_MORPH3 ]
  map to morph targets 0..3 on each primitive (extras on asset are ignored).

Supported:
- glTF JSON starting with '{' (buffers as data: URIs)
- Binary .glb (JSON chunk + BIN chunk)
- Triangle lists, float POSITION, optional float TEXCOORD_0, u16/u32 indices
- Morph targets with POSITION deltas (same vertex count)

Not supported: Draco, sparse accessors, external http/file buffer URIs, skins/animation.

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "Model_local.h"

static const char *GLTF_SNAPSHOT_NAME = "_GLTF_Snapshot_";

namespace {

static void SkipWs( const char *&p ) {
	while ( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' ) {
		++p;
	}
}

static bool ParseUInt( const char *&p, unsigned &out ) {
	SkipWs( p );
	if ( *p < '0' || *p > '9' ) {
		return false;
	}
	unsigned v = 0;
	while ( *p >= '0' && *p <= '9' ) {
		v = v * 10u + ( unsigned )( *p - '0' );
		++p;
	}
	out = v;
	return true;
}

static bool ParseInt( const char *&p, int &out ) {
	SkipWs( p );
	bool neg = false;
	if ( *p == '-' ) {
		neg = true;
		++p;
	}
	unsigned u = 0;
	if ( !ParseUInt( p, u ) ) {
		return false;
	}
	out = neg ? -( int )u : ( int )u;
	return true;
}

static const char *FindKeyValue( const char *objStart, const char *objEnd, const char *key ) {
	const idStr needle = idStr( "\"" ) + key + "\"";
	const char *q = objStart;
	while ( q < objEnd ) {
		q = strstr( q, needle.c_str() );
		if ( !q || q >= objEnd ) {
			return NULL;
		}
		const char *t = q + needle.Length();
		SkipWs( t );
		if ( *t != ':' ) {
			q += 1;
			continue;
		}
		++t;
		SkipWs( t );
		return t;
	}
	return NULL;
}

static const char *SkipJsonValue( const char *p, const char *end ) {
	SkipWs( p );
	if ( p >= end ) {
		return end;
	}
	if ( *p == '{' ) {
		int d = 0;
		for ( ; p < end; ++p ) {
			if ( *p == '{' ) {
				d++;
			} else if ( *p == '}' ) {
				if ( --d == 0 ) {
					return p + 1;
				}
			}
		}
		return end;
	}
	if ( *p == '[' ) {
		int d = 0;
		for ( ; p < end; ++p ) {
			if ( *p == '[' ) {
				d++;
			} else if ( *p == ']' ) {
				if ( --d == 0 ) {
					return p + 1;
				}
			}
		}
		return end;
	}
	if ( *p == '"' ) {
		++p;
		while ( p < end && *p != '"' ) {
			if ( *p == '\\' && p + 1 < end ) {
				p += 2;
			} else {
				++p;
			}
		}
		if ( p < end && *p == '"' ) {
			++p;
		}
		return p;
	}
	while ( p < end && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' ) {
		++p;
	}
	return p;
}

static bool ExtractTopArray( const char *json, int jsonLen, const char *key, const char *&outBegin, const char *&outEnd ) {
	const idStr head = idStr( "\"" ) + key + "\"";
	const char *root = json;
	const char *end = json + jsonLen;
	const char *h = strstr( root, head.c_str() );
	if ( !h || h >= end ) {
		return false;
	}
	const char *p = h + head.Length();
	SkipWs( p );
	if ( *p != ':' ) {
		return false;
	}
	++p;
	SkipWs( p );
	if ( *p != '[' ) {
		return false;
	}
	++p;
	outBegin = p;
	int depth = 1;
	for ( ; p < end; ++p ) {
		if ( *p == '[' ) {
			depth++;
		} else if ( *p == ']' ) {
			if ( --depth == 0 ) {
				outEnd = p;
				return true;
			}
		}
	}
	return false;
}

static bool NextObjectInArray( const char *&cursor, const char *arrayEnd, const char *&objBegin, const char *&objEnd ) {
	SkipWs( cursor );
	if ( cursor >= arrayEnd || *cursor == ']' ) {
		return false;
	}
	if ( *cursor == ',' ) {
		++cursor;
		SkipWs( cursor );
	}
	if ( *cursor != '{' ) {
		return false;
	}
	objBegin = cursor;
	int depth = 0;
	const char *p = cursor;
	for ( ; p < arrayEnd; ++p ) {
		if ( *p == '{' ) {
			depth++;
		} else if ( *p == '}' ) {
			depth--;
			if ( depth == 0 ) {
				objEnd = p + 1;
				cursor = objEnd;
				SkipWs( cursor );
				if ( *cursor == ',' ) {
					++cursor;
				}
				return true;
			}
		}
	}
	return false;
}

struct AccessorInfo {
	int bufferView = -1;
	int byteOffset = 0;
	int count = 0;
	int componentType = 0;
	int numComponents = 0;
};

static bool ParseAccessorByIndex( const char *json, int jsonLen, int index, AccessorInfo &info ) {
	const char *ab = NULL, *ae = NULL;
	if ( !ExtractTopArray( json, jsonLen, "accessors", ab, ae ) ) {
		return false;
	}
	const char *cur = ab;
	for ( int i = 0; i <= index; ++i ) {
		const char *ob = NULL, *oe = NULL;
		if ( !NextObjectInArray( cur, ae, ob, oe ) ) {
			return false;
		}
		if ( i == index ) {
			const char *bv = FindKeyValue( ob, oe, "bufferView" );
			if ( bv ) {
				ParseInt( bv, info.bufferView );
			}
			const char *bo = FindKeyValue( ob, oe, "byteOffset" );
			if ( bo ) {
				ParseInt( bo, info.byteOffset );
			}
			const char *cnt = FindKeyValue( ob, oe, "count" );
			if ( !cnt || !ParseInt( cnt, info.count ) ) {
				return false;
			}
			const char *ct = FindKeyValue( ob, oe, "componentType" );
			if ( ct ) {
				ParseInt( ct, info.componentType );
			}
			const char *typ = FindKeyValue( ob, oe, "type" );
			if ( typ && *typ == '"' ) {
				++typ;
				if ( !strncmp( typ, "SCALAR", 6 ) ) {
					info.numComponents = 1;
				} else if ( !strncmp( typ, "VEC2", 4 ) ) {
					info.numComponents = 2;
				} else if ( !strncmp( typ, "VEC3", 4 ) ) {
					info.numComponents = 3;
				} else if ( !strncmp( typ, "VEC4", 4 ) ) {
					info.numComponents = 4;
				}
			}
			return info.bufferView >= 0 && info.count > 0 && info.numComponents > 0;
		}
	}
	return false;
}

static bool BufferViewMeta( const char *json, int jsonLen, int bvIndex, int &outBuffer, int &outByteOffset, int &outByteLength, int &outStride ) {
	outStride = 0;
	const char *bb = NULL, *be = NULL;
	if ( !ExtractTopArray( json, jsonLen, "bufferViews", bb, be ) ) {
		return false;
	}
	const char *cur = bb;
	for ( int i = 0; i <= bvIndex; ++i ) {
		const char *ob = NULL, *oe = NULL;
		if ( !NextObjectInArray( cur, be, ob, oe ) ) {
			return false;
		}
		if ( i == bvIndex ) {
			const char *b = FindKeyValue( ob, oe, "buffer" );
			if ( !b || !ParseInt( b, outBuffer ) ) {
				return false;
			}
			outByteOffset = 0;
			const char *o = FindKeyValue( ob, oe, "byteOffset" );
			if ( o ) {
				ParseInt( o, outByteOffset );
			}
			const char *l = FindKeyValue( ob, oe, "byteLength" );
			if ( !l || !ParseInt( l, outByteLength ) ) {
				return false;
			}
			const char *st = FindKeyValue( ob, oe, "byteStride" );
			if ( st ) {
				ParseInt( st, outStride );
			}
			return true;
		}
	}
	return false;
}

static int AccessorStride( const char *json, int jsonLen, const AccessorInfo &acc ) {
	int buf, bo, blen, stride;
	if ( !BufferViewMeta( json, jsonLen, acc.bufferView, buf, bo, blen, stride ) ) {
		return acc.numComponents * 4;
	}
	if ( stride > 0 ) {
		return stride;
	}
	return acc.numComponents * 4;
}

static bool DecodeDataUri( const char *uri, int uriLen, byte *&outData, int &outLen ) {
	const char *base = strstr( uri, "base64," );
	if ( !base || base > uri + uriLen ) {
		return false;
	}
	base += 7;
	idStr b64( base, 0, ( int )( uri + uriLen - base ) );
	b64.StripTrailingWhitespace();
	idBase64 dec( b64 );
	outLen = dec.DecodeLength();
	if ( outLen <= 0 ) {
		return false;
	}
	outData = (byte *)Mem_Alloc( outLen );
	dec.Decode( outData );
	return true;
}

static bool LoadBufferBytes( const char *json, int jsonLen, int bufferIndex, const byte *binChunk, int binChunkLen, byte *&outData, int &outLen ) {
	const char *bb = NULL, *be = NULL;
	if ( !ExtractTopArray( json, jsonLen, "buffers", bb, be ) ) {
		return false;
	}
	const char *cur = bb;
	for ( int i = 0; i <= bufferIndex; ++i ) {
		const char *ob = NULL, *oe = NULL;
		if ( !NextObjectInArray( cur, be, ob, oe ) ) {
			return false;
		}
		if ( i == bufferIndex ) {
			const char *uri = FindKeyValue( ob, oe, "uri" );
			if ( uri && *uri == '"' ) {
				++uri;
				const char *ue = uri;
				while ( ue < oe && *ue != '"' ) {
					++ue;
				}
				return DecodeDataUri( uri, ( int )( ue - uri ), outData, outLen );
			}
			// GLB BIN chunk
			const char *bl = FindKeyValue( ob, oe, "byteLength" );
			if ( binChunk && binChunkLen > 0 && bl ) {
				int byteLen = 0;
				const char *q = bl;
				if ( ParseInt( q, byteLen ) && byteLen > 0 && byteLen <= binChunkLen ) {
					outData = (byte *)Mem_Alloc( byteLen );
					memcpy( outData, binChunk, byteLen );
					outLen = byteLen;
					return true;
				}
			}
			return false;
		}
	}
	return false;
}

static void FreeBufferBytes( byte *p, int len ) {
	( void )len;
	if ( p ) {
		Mem_Free( p );
	}
}

static bool ReadFloats( const byte *buf, int bufLen, int baseOff, int stride, int count, int nComp, idList<float> &out ) {
	const int need = count * nComp;
	out.SetNum( need );
	for ( int i = 0; i < count; ++i ) {
		const int off = baseOff + i * stride;
		if ( off + nComp * 4 > bufLen ) {
			return false;
		}
		const float *f = ( const float * )( buf + off );
		for ( int c = 0; c < nComp; ++c ) {
			out[i * nComp + c] = f[c];
		}
	}
	return true;
}

static bool ReadIndices( const byte *buf, int bufLen, int baseOff, int stride, int count, int compType, idList<unsigned> &out ) {
	out.SetNum( count );
	for ( int i = 0; i < count; ++i ) {
		const int off = baseOff + i * stride;
		unsigned v = 0;
		if ( compType == 5125 ) {
			if ( off + 4 > bufLen ) {
				return false;
			}
			v = *( const unsigned * )( buf + off );
		} else if ( compType == 5123 ) {
			if ( off + 2 > bufLen ) {
				return false;
			}
			v = *( const unsigned short * )( buf + off );
		} else if ( compType == 5121 ) {
			if ( off + 1 > bufLen ) {
				return false;
			}
			v = buf[off];
		} else {
			return false;
		}
		out[i] = v;
	}
	return true;
}

} // namespace

/*
====================
idRenderModelGlTF::LoadFromBuffer
====================
*/
bool idRenderModelGlTF::LoadFromBuffer( const byte *data, int len, const char *pathForErrors ) {
	meshes.Clear();

	if ( len < 8 ) {
		return false;
	}

	idStr jsonText;
	const byte *binChunk = NULL;
	int binChunkLen = 0;

	if ( data[0] == '{' ) {
		jsonText = idStr( ( const char * )data, 0, len );
	} else if ( len >= 12 && memcmp( data, "glTF", 4 ) == 0 ) {
		const unsigned total = *( const unsigned * )( data + 8 );
		( void )total;
		size_t off = 12;
		while ( off + 8 <= ( size_t )len ) {
			const unsigned chunkLen = *( const unsigned * )( data + off );
			const unsigned chunkType = *( const unsigned * )( data + off + 4 );
			off += 8;
			if ( off + chunkLen > ( size_t )len ) {
				break;
			}
			if ( chunkType == 0x4E4F534A ) {
				jsonText = idStr( ( const char * )( data + off ), 0, ( int )chunkLen );
			} else if ( chunkType == 0x004E4942 ) {
				binChunk = data + off;
				binChunkLen = ( int )chunkLen;
			}
			off += chunkLen;
		}
	} else {
		common->Warning( "GlTF: unrecognized header in %s", pathForErrors );
		return false;
	}

	const char *json = jsonText.c_str();
	const int jsonLen = jsonText.Length();

	const char *meshArrBegin = NULL, *meshArrEnd = NULL;
	if ( !ExtractTopArray( json, jsonLen, "meshes", meshArrBegin, meshArrEnd ) ) {
		common->Warning( "GlTF: no meshes[] in %s", pathForErrors );
		return false;
	}

	const char *mCur = meshArrBegin;
	while ( true ) {
		const char *meshOb = NULL, *meshOe = NULL;
		if ( !NextObjectInArray( mCur, meshArrEnd, meshOb, meshOe ) ) {
			break;
		}
		const char *primPtr = FindKeyValue( meshOb, meshOe, "primitives" );
		if ( !primPtr || *primPtr != '[' ) {
			continue;
		}
		++primPtr;
		const char *primArrEnd = primPtr;
		int depth = 1;
		for ( ; primArrEnd < meshOe; ++primArrEnd ) {
			if ( *primArrEnd == '[' ) {
				depth++;
			} else if ( *primArrEnd == ']' ) {
				if ( --depth == 0 ) {
					break;
				}
			}
		}
		const char *pCur = primPtr;
		while ( true ) {
			const char *primBegin = NULL, *primEnd = NULL;
			if ( !NextObjectInArray( pCur, primArrEnd, primBegin, primEnd ) ) {
				break;
			}

			gltfMeshData_t mesh;
			mesh.materialName = "_white";

			int posAcc = -1, texAcc = -1, idxAcc = -1;
			const char *attr = FindKeyValue( primBegin, primEnd, "attributes" );
			if ( attr && *attr == '{' ) {
				const char *aEnd = SkipJsonValue( attr, primEnd );
				const char *pp = FindKeyValue( attr, aEnd, "POSITION" );
				if ( pp ) {
					ParseInt( pp, posAcc );
				}
				const char *tp = FindKeyValue( attr, aEnd, "TEXCOORD_0" );
				if ( !tp ) {
					tp = FindKeyValue( attr, aEnd, "TEXCOORD0" );
				}
				if ( tp ) {
					ParseInt( tp, texAcc );
				}
			}
			const char *ip = FindKeyValue( primBegin, primEnd, "indices" );
			if ( ip ) {
				ParseInt( ip, idxAcc );
			}

			if ( posAcc < 0 || idxAcc < 0 ) {
				continue;
			}

			AccessorInfo accPos, accIdx, accUv;
			if ( !ParseAccessorByIndex( json, jsonLen, posAcc, accPos ) || accPos.componentType != 5126 || accPos.numComponents != 3 ) {
				continue;
			}
			if ( !ParseAccessorByIndex( json, jsonLen, idxAcc, accIdx ) ) {
				continue;
			}
			bool haveUv = false;
			if ( texAcc >= 0 ) {
				haveUv = ParseAccessorByIndex( json, jsonLen, texAcc, accUv ) && accUv.componentType == 5126 && accUv.numComponents == 2;
			}

			int posBuf = 0, posOff = 0, posLen = 0, posStrideMeta = 0;
			int idxBuf = 0, idxOff = 0, idxLen = 0, idxStrideMeta = 0;
			int uvBuf = 0, uvOff = 0, uvLen = 0, uvStrideMeta = 0;
			if ( !BufferViewMeta( json, jsonLen, accPos.bufferView, posBuf, posOff, posLen, posStrideMeta ) ) {
				continue;
			}
			if ( !BufferViewMeta( json, jsonLen, accIdx.bufferView, idxBuf, idxOff, idxLen, idxStrideMeta ) ) {
				continue;
			}

			byte *bufPos = NULL, *bufIdx = NULL, *bufUv = NULL;
			int lenPos = 0, lenIdx = 0, lenUv = 0;
			if ( !LoadBufferBytes( json, jsonLen, posBuf, binChunk, binChunkLen, bufPos, lenPos ) ) {
				continue;
			}
			if ( !LoadBufferBytes( json, jsonLen, idxBuf, binChunk, binChunkLen, bufIdx, lenIdx ) ) {
				FreeBufferBytes( bufPos, lenPos );
				continue;
			}
			if ( haveUv ) {
				if ( !BufferViewMeta( json, jsonLen, accUv.bufferView, uvBuf, uvOff, uvLen, uvStrideMeta ) ) {
					haveUv = false;
				} else if ( !LoadBufferBytes( json, jsonLen, uvBuf, binChunk, binChunkLen, bufUv, lenUv ) ) {
					haveUv = false;
				}
			}

			const int posStride = AccessorStride( json, jsonLen, accPos );
			const int idxStride = AccessorStride( json, jsonLen, accIdx );
			int uvStride = 8;
			if ( haveUv ) {
				uvStride = AccessorStride( json, jsonLen, accUv );
			}

			idList<float> positions, uvs;
			idList<unsigned> indices;
			if ( !ReadFloats( bufPos, lenPos, posOff + accPos.byteOffset, posStride, accPos.count, 3, positions ) ) {
				FreeBufferBytes( bufPos, lenPos );
				FreeBufferBytes( bufIdx, lenIdx );
				FreeBufferBytes( bufUv, lenUv );
				continue;
			}
			if ( haveUv && !ReadFloats( bufUv, lenUv, uvOff + accUv.byteOffset, uvStride, accUv.count, 2, uvs ) ) {
				haveUv = false;
			}
			if ( !ReadIndices( bufIdx, lenIdx, idxOff + accIdx.byteOffset, idxStride, accIdx.count, accIdx.componentType, indices ) ) {
				FreeBufferBytes( bufPos, lenPos );
				FreeBufferBytes( bufIdx, lenIdx );
				FreeBufferBytes( bufUv, lenUv );
				continue;
			}

			FreeBufferBytes( bufPos, lenPos );
			FreeBufferBytes( bufIdx, lenIdx );
			FreeBufferBytes( bufUv, lenUv );

			mesh.baseVerts.SetNum( accPos.count );
			for ( int vi = 0; vi < accPos.count; ++vi ) {
				idDrawVert &v = mesh.baseVerts[vi];
				v.Clear();
				v.xyz[0] = positions[vi * 3 + 0];
				v.xyz[1] = positions[vi * 3 + 1];
				v.xyz[2] = positions[vi * 3 + 2];
				if ( haveUv && vi * 2 + 1 < uvs.Num() ) {
					v.st[0] = uvs[vi * 2 + 0];
					v.st[1] = uvs[vi * 2 + 1];
				}
			}
			mesh.indices = indices;

			const char *targetsPtr = FindKeyValue( primBegin, primEnd, "targets" );
			if ( targetsPtr && *targetsPtr == '[' ) {
				++targetsPtr;
				const char *tEnd = targetsPtr;
				int td = 1;
				for ( ; tEnd < primEnd; ++tEnd ) {
					if ( *tEnd == '[' ) {
						td++;
					} else if ( *tEnd == ']' ) {
						if ( --td == 0 ) {
							break;
						}
					}
				}
				const char *tCur = targetsPtr;
				while ( true ) {
					const char *mtBegin = NULL, *mtEnd = NULL;
					if ( !NextObjectInArray( tCur, tEnd, mtBegin, mtEnd ) ) {
						break;
					}
					const char *dp = FindKeyValue( mtBegin, mtEnd, "POSITION" );
					int dacc = -1;
					if ( dp ) {
						ParseInt( dp, dacc );
					}
					if ( dacc < 0 ) {
						continue;
					}
					AccessorInfo dInfo;
					if ( !ParseAccessorByIndex( json, jsonLen, dacc, dInfo ) || dInfo.componentType != 5126 || dInfo.numComponents != 3 || dInfo.count != accPos.count ) {
						continue;
					}
					int db = 0, dbo = 0, dbl = 0, ds = 0;
					if ( !BufferViewMeta( json, jsonLen, dInfo.bufferView, db, dbo, dbl, ds ) ) {
						continue;
					}
					byte *dbuf = NULL;
					int dlen = 0;
					if ( !LoadBufferBytes( json, jsonLen, db, binChunk, binChunkLen, dbuf, dlen ) ) {
						continue;
					}
					const int dStride = AccessorStride( json, jsonLen, dInfo );
					idList<float> deltas;
					if ( !ReadFloats( dbuf, dlen, dbo + dInfo.byteOffset, dStride, dInfo.count, 3, deltas ) ) {
						FreeBufferBytes( dbuf, dlen );
						continue;
					}
					FreeBufferBytes( dbuf, dlen );
					gltfMorphTarget_t mt;
					mt.positionDeltas.SetNum( accPos.count );
					for ( int vi = 0; vi < accPos.count; ++vi ) {
						mt.positionDeltas[vi].Set( deltas[vi * 3 + 0], deltas[vi * 3 + 1], deltas[vi * 3 + 2] );
					}
					mesh.morphTargets.Append( mt );
				}
			}

			meshes.Append( mesh );
		}
	}

	if ( meshes.Num() == 0 ) {
		common->Warning( "GlTF: no loadable primitives in %s", pathForErrors );
		return false;
	}
	return true;
}

void idRenderModelGlTF::BuildMeshSurface( const gltfMeshData_t &mesh, const float *morphWeights, int numMorphWeights, modelSurface_t *surf ) const {
	const int nv = mesh.baseVerts.Num();
	const int ni = mesh.indices.Num();

	srfTriangles_t *tri = surf->geometry;
	if ( !tri || tri->numVerts != nv || tri->numIndexes != ni ) {
		if ( tri ) {
			R_FreeStaticTriSurfVertexCaches( tri );
			R_FreeStaticTriSurf( tri );
		}
		surf->geometry = R_AllocStaticTriSurf();
		tri = surf->geometry;
		R_AllocStaticTriSurfVerts( tri, nv );
		R_AllocStaticTriSurfIndexes( tri, ni );
		tri->numVerts = nv;
		tri->numIndexes = ni;
	}

	tri->deformedSurface = true;
	tri->tangentsCalculated = false;
	tri->facePlanesCalculated = false;
	tri->isSkeletal = false;

	memcpy( tri->indexes, mesh.indices.Ptr(), ni * sizeof( tri->indexes[0] ) );

	for ( int v = 0; v < nv; ++v ) {
		tri->verts[v] = mesh.baseVerts[v];
	}

	const int nm = mesh.morphTargets.Num() < numMorphWeights ? mesh.morphTargets.Num() : numMorphWeights;
	for ( int m = 0; m < nm; ++m ) {
		float w = morphWeights[m];
		if ( w <= 0.0f ) {
			continue;
		}
		if ( w > 1.0f ) {
			w = 1.0f;
		}
		const idList<idVec3> &d = mesh.morphTargets[m].positionDeltas;
		if ( d.Num() != nv ) {
			continue;
		}
		for ( int v = 0; v < nv; ++v ) {
			tri->verts[v].xyz += d[v] * w;
		}
	}

	R_BoundTriSurf( tri );
	if ( !r_useDeferredTangents.GetBool() ) {
		R_DeriveTangents( tri );
	}
}

idBounds idRenderModelGlTF::CalcDeformedBounds( const float *morphWeights, int numMorphWeights ) const {
	idBounds b;
	b.Clear();
	for ( int mi = 0; mi < meshes.Num(); ++mi ) {
		const gltfMeshData_t &mesh = meshes[mi];
		const int nv = mesh.baseVerts.Num();
		for ( int v = 0; v < nv; ++v ) {
			idVec3 p = mesh.baseVerts[v].xyz;
			const int nm = mesh.morphTargets.Num() < numMorphWeights ? mesh.morphTargets.Num() : numMorphWeights;
			for ( int m = 0; m < nm; ++m ) {
				float w = morphWeights[m];
				if ( w > 1.0f ) {
					w = 1.0f;
				}
				if ( w > 0.0f && mesh.morphTargets[m].positionDeltas.Num() == nv ) {
					p += mesh.morphTargets[m].positionDeltas[v] * w;
				}
			}
			b.AddPoint( p );
		}
	}
	return b;
}

void idRenderModelGlTF::InitFromFile( const char *fileName ) {
	name = fileName;
	LoadModel();
}

void idRenderModelGlTF::LoadModel() {
	if ( !purged ) {
		PurgeModel();
	}
	purged = false;

	byte *buf = NULL;
	const int n = fileSystem->ReadFile( name, (void **)&buf, &timeStamp );
	if ( n <= 0 || !buf ) {
		common->Warning( "idRenderModelGlTF::LoadModel: could not load '%s'", name.c_str() );
		MakeDefaultModel();
		return;
	}

	if ( !LoadFromBuffer( buf, n, name.c_str() ) ) {
		fileSystem->FreeFile( buf );
		meshes.Clear();
		MakeDefaultModel();
		return;
	}
	fileSystem->FreeFile( buf );

	float zero[4] = { 0, 0, 0, 0 };
	for ( int i = 0; i < meshes.Num(); ++i ) {
		modelSurface_t surf;
		surf.id = i;
		surf.shader = declManager->FindMaterial( meshes[i].materialName );
		surf.geometry = R_AllocStaticTriSurf();
		BuildMeshSurface( meshes[i], zero, 0, &surf );
		AddSurface( surf );
	}

	FinishSurfaces();
	reloadable = true;
}

void idRenderModelGlTF::PurgeModel() {
	meshes.Clear();
	idRenderModelStatic::PurgeModel();
}

void idRenderModelGlTF::TouchData() {
	for ( int i = 0; i < meshes.Num(); ++i ) {
		declManager->FindMaterial( meshes[i].materialName );
	}
}

dynamicModel_t idRenderModelGlTF::IsDynamicModel() const {
	return DM_CACHED;
}

idRenderModel *idRenderModelGlTF::InstantiateDynamicModel( const struct renderEntity_s *ent, const struct viewDef_s *view, idRenderModel *cachedModel ) {
	( void )view;

	if ( cachedModel && !r_useCachedDynamicModels.GetBool() ) {
		delete cachedModel;
		cachedModel = NULL;
	}

	if ( purged ) {
		common->DWarning( "GlTF model %s instantiated while purged", Name() );
		LoadModel();
	}

	idRenderModelStatic *staticModel = NULL;
	if ( cachedModel ) {
		assert( dynamic_cast<idRenderModelStatic *>( cachedModel ) != NULL );
		assert( idStr::Icmp( cachedModel->Name(), GLTF_SNAPSHOT_NAME ) == 0 );
		staticModel = static_cast<idRenderModelStatic *>( cachedModel );
	} else {
		staticModel = new idRenderModelStatic;
		staticModel->InitEmpty( GLTF_SNAPSHOT_NAME );
	}

	staticModel->bounds.Clear();

	float wts[4];
	wts[0] = ent->shaderParms[SHADERPARM_GLTF_MORPH0];
	wts[1] = ent->shaderParms[SHADERPARM_GLTF_MORPH1];
	wts[2] = ent->shaderParms[SHADERPARM_GLTF_MORPH2];
	wts[3] = ent->shaderParms[SHADERPARM_GLTF_MORPH3];

	int maxMorphs = 0;
	for ( int i = 0; i < meshes.Num(); ++i ) {
		const int nm = meshes[i].morphTargets.Num();
		maxMorphs = nm > maxMorphs ? nm : maxMorphs;
	}
	const int numW = maxMorphs < 4 ? maxMorphs : 4;

	for ( int i = 0; i < meshes.Num(); ++i ) {
		const idMaterial *shader = declManager->FindMaterial( meshes[i].materialName );
		const idMaterial *useShader = R_RemapShaderBySkin( shader, ent->customSkin, ent->customShader );
		if ( !useShader || ( !useShader->IsDrawn() && !useShader->SurfaceCastsShadow() ) ) {
			staticModel->DeleteSurfaceWithId( i );
			continue;
		}

		int surfaceNum = -1;
		modelSurface_t *surf = NULL;
		if ( staticModel->FindSurfaceWithId( i, surfaceNum ) ) {
			surf = &staticModel->surfaces[surfaceNum];
		} else {
			idRenderModelOverlay::RemoveOverlaySurfacesFromModel( staticModel );
			surf = &staticModel->surfaces.Alloc();
			surf->geometry = NULL;
			surf->id = i;
		}
		surf->shader = useShader;
		BuildMeshSurface( meshes[i], wts, numW, surf );
		staticModel->bounds.AddBounds( surf->geometry->bounds );
	}

	return staticModel;
}

idBounds idRenderModelGlTF::Bounds( const struct renderEntity_s *ent ) const {
	if ( !ent ) {
		return bounds;
	}
	float wts[4];
	wts[0] = ent->shaderParms[SHADERPARM_GLTF_MORPH0];
	wts[1] = ent->shaderParms[SHADERPARM_GLTF_MORPH1];
	wts[2] = ent->shaderParms[SHADERPARM_GLTF_MORPH2];
	wts[3] = ent->shaderParms[SHADERPARM_GLTF_MORPH3];
	int maxMorphs = 0;
	for ( int i = 0; i < meshes.Num(); ++i ) {
		const int nm = meshes[i].morphTargets.Num();
		maxMorphs = nm > maxMorphs ? nm : maxMorphs;
	}
	const int numW = maxMorphs < 4 ? maxMorphs : 4;
	return CalcDeformedBounds( wts, numW );
}

void idRenderModelGlTF::Print() const {
	common->Printf( "%s (GlTF)\n", name.c_str() );
	for ( int i = 0; i < meshes.Num(); ++i ) {
		common->Printf( " mesh %i: verts %i tris %i morphs %i mat %s\n",
			i, meshes[i].baseVerts.Num(), meshes[i].indices.Num() / 3, meshes[i].morphTargets.Num(),
			meshes[i].materialName.c_str() );
	}
	common->Printf( " morph: shaderParms[%i..%i] = weights\n", SHADERPARM_GLTF_MORPH0, SHADERPARM_GLTF_MORPH3 );
}

void idRenderModelGlTF::List() const {
	int v = 0, t = 0;
	for ( int i = 0; i < meshes.Num(); ++i ) {
		v += meshes[i].baseVerts.Num();
		t += meshes[i].indices.Num() / 3;
	}
	common->Printf( " %4ik %3i %4i %4i %s(GlTF)\n", Memory() / 1024, meshes.Num(), v, t, Name() );
}

int idRenderModelGlTF::Memory() const {
	int m = idRenderModelStatic::Memory();
	for ( int i = 0; i < meshes.Num(); ++i ) {
		m += ( int )meshes[i].baseVerts.Allocated();
		m += ( int )meshes[i].indices.Allocated();
		for ( int j = 0; j < meshes[i].morphTargets.Num(); ++j ) {
			m += ( int )meshes[i].morphTargets[j].positionDeltas.Allocated();
		}
	}
	return m;
}
