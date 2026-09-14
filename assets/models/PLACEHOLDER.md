# AR-Helmet — Modelos 3D

Coloca aquí los archivos de geometría del casco y del occluder facial.

## Archivos requeridos

| Archivo              | Descripción                                           |
|----------------------|-------------------------------------------------------|
| `helmet.obj`         | Malla 3D del casco (+ archivo `.mtl` y texturas)     |
| `face_occluder.obj`  | Malla del rostro para máscara de oclusión en Z-buffer |

## Especificaciones de los modelos

### `helmet.obj`
- Sistema de coordenadas: **Y-up, Z-forward** (estándar OBJ)
- Origen centrado en la cabeza (a la altura de las sienes)
- Escala en **milímetros** (para coherencia con el modelo de PnP)
- Con normales automáticas (Assimp las generará con `aiProcess_GenSmoothNormals`)
- UVs opcionales (si incluye texturas)

### `face_occluder.obj`
- Malla simplificada del rostro humano genérico
- Solo geometría (sin texturas ni materiales)
- Mismo sistema de coordenadas y escala que `helmet.obj`
- Resolución baja (~500-2000 vértices)

## Fuentes sugeridas

- [Sketchfab](https://sketchfab.com) — Buscar modelos de cascos con licencia CC
- [Thingiverse](https://thingiverse.com) — Modelos STL convertibles a OBJ
- `blender --background --python convert.py` — Convertir desde STL/FBX
- [MediaPipe FaceGeometry](https://github.com/google/mediapipe/tree/master/mediapipe/modules/face_geometry) — Face mesh genérico para occluder

## Conversión rápida con Blender CLI

```bash
blender --background --python - << 'EOF'
import bpy
bpy.ops.import_scene.fbx(filepath="input.fbx")
bpy.ops.export_scene.obj(filepath="helmet.obj", use_materials=True)
EOF
```

## Verificación

```python
import ar_helmet_core as core
m = core.Renderer.__doc__   # Verificar que el módulo cargó
```

> **Nota:** La ausencia de estos archivos no impide compilar `core_engine`.
> El `Renderer` verifica la existencia en `init()` y lanza `std::runtime_error`
> si no encuentra los modelos. El test de Semana 5 (`tests/test_pose.py`)
> **no requiere** estos archivos.
