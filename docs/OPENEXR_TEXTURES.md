# OpenEXR (`.exr`) — 32-bit float textures

This fork loads **OpenEXR** files via **[TinyEXR](https://github.com/syoyo/tinyexr)** (header + implementation in `neo/renderer/R_LoadEXR.cpp`, bundled headers under `neo/renderer/third_party/tinyexr/`).

## Material usage

Reference an EXR like any other image path. The loader upgrades the texture to **linear RGBA float** (`GL_RGBA_FLOAT32_ATI` / `GL_RGBA32F_ATI`) when the file extension is **`.exr`**, or when the stage requests **`hdrfloat`** / **`hdr`**:

```text
textures/my_env
{
    qer_editorimage textures/my_env.exr
    {
        blend diffusemap
        map textures/my_env.exr hdrfloat
    }
}
```

If you omit **`hdrfloat`**, paths ending in **`.exr`** still pick **`TD_HDR_FLOAT`** automatically (`idImageManager::ImageFromFile`).

## Image programs

**`heightmap`**, **`addnormals`**, **`smoothnormals`**, **`add`**, **`scale`**, **`invertAlpha`**, **`invertColor`**, **`makeIntensity`**, and **`makeAlpha`** are **not** supported on float EXR sources (they assume 8-bit RGBA). Use plain **`map path.exr`** (or a trivial image program that only references the file).

## Requirements

- **Power-of-two** dimensions (same as other engine textures).
- **MSVC:** `neo/_DoomDLL.props` adds `renderer\third_party\tinyexr` to the include path; **`miniz.c`** is compiled without a precompiled header.

## Licensing

TinyEXR and bundled OpenEXR/miniz portions carry their respective licenses in the upstream headers (`tinyexr.h`, `miniz.h`).
