# AR-Helmet

**Aplicación de Realidad Aumentada con casco 3D renderizado sobre el rostro.**

Curso: CS4016 — Computación Gráfica  
Arquitectura: Python (UI/captura) + C++ (OpenGL 3.3 Core) vía pybind11

---

## Arquitectura del Sistema

```
┌─────────────────────────────────────────────────────────────┐
│  app_ui/ (Python)                                           │
│  ┌──────────┐  ┌──────────────────┐  ┌───────────────────┐ │
│  │ camera.py│→ │landmark_detector │→ │    main.py        │ │
│  │ OpenCV   │  │ MediaPipe+solvePnP│  │  Orquestador Qt  │ │
│  └──────────┘  └──────────────────┘  └────────┬──────────┘ │
│                                                │ pybind11   │
│  ┌─────────────────────────────────────────────┼──────────┐ │
│  │  core_engine/ (C++17 + OpenGL 3.3)          │          │ │
│  │  ┌───────────────┐  ┌────────────────┐      │          │ │
│  │  │ PoseEstimator │  │   Renderer     │←─────┘          │ │
│  │  │ (Rodrigues)   │  │ 3-pass pipeline│                 │ │
│  │  └───────────────┘  └────────────────┘                 │ │
│  │  ┌──────────────────────────────────┐                  │ │
│  │  │ LightEstimator (EQUIPO HUMANO)   │ ← TODO           │ │
│  │  └──────────────────────────────────┘                  │ │
│  └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

---

## Requisitos del Sistema

### Python
- Python 3.10 – 3.14+ (probado en 3.11 y 3.14)
- pip 23+

> **Windows**: usar `py -3` en lugar de `python` si el intérprete no está en PATH.

### Compilador C++
- **Windows**: Visual Studio 2022 (MSVC 17+) o MinGW-w64 con GCC 12+
- **Linux**: GCC 12+ o Clang 15+

### Herramientas de Build
- CMake 3.20+
- Git (para FetchContent)
- Conexión a internet (primera compilación descarga dependencias)

---

## Instalación Rápida

### 1. Clonar e instalar dependencias Python

```bash
git clone <url-repositorio>
cd ar-helmet-project

# Linux / macOS
pip install -r requirements.txt

# Windows (si 'python' no está en PATH)
py -3 -m pip install -r requirements.txt
```

### 2. Compilar el módulo C++

**Linux / macOS:**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(nproc)
```

**Windows (MSVC):**
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

**Windows (MinGW):**
```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

El módulo `ar_helmet_core.pyd` (Windows) o `ar_helmet_core.so` (Linux)
se copiará automáticamente a la raíz del proyecto.

### 3. Verificar instalación

```python
import ar_helmet_core as core
print(core.__version__)          # "0.1.0"
print(dir(core))                 # [Renderer, PoseEstimator, LightEstimator, ...]
```

---

## Ejecución

### Test Semana 5 (sin C++ requerido)

Valida el pipeline Python puro: cámara + landmarks + PnP + ejes 3D.

```bash
python tests/test_pose.py
python tests/test_pose.py --camera 1 --width 1920 --height 1080
python tests/test_pose.py --no-gui --output output.mp4   # Headless
```

**Controles en ventana:**
| Tecla | Acción                   |
|-------|--------------------------|
| `Q`   | Salir                    |
| `D`   | Toggle face mesh debug   |
| `S`   | Guardar captura PNG      |
| `ESC` | Salir                    |

### Aplicación completa (requiere C++ compilado)

```bash
python app_ui/main.py
python app_ui/main.py --camera 0 --width 1280 --height 720 --fps 60
```

---

## Estructura del Proyecto

```
ar-helmet-project/
├── CMakeLists.txt                  ← Raíz: proxy a core_engine
├── core_engine/
│   ├── CMakeLists.txt              ← Build principal (FetchContent)
│   ├── include/
│   │   ├── Shader.hpp              ← RAII wrapper GLSL
│   │   ├── Mesh.hpp                ← VAO/VBO/EBO + vertex layout
│   │   ├── Model.hpp               ← Cargador Assimp
│   │   ├── PoseEstimator.hpp       ← Rodrigues + conv. OpenCV→OpenGL
│   │   ├── LightEstimator.hpp      ← INTERFAZ (equipo humano)
│   │   └── Renderer.hpp            ← Pipeline 3 pasadas + FBO
│   ├── src/
│   │   ├── Shader.cpp
│   │   ├── Model.cpp               ← Assimp + stb_image
│   │   ├── PoseEstimator.cpp       ← Rodrigues analítico sin OpenCV
│   │   ├── LightEstimator.cpp      ← STUBS (equipo humano)
│   │   └── Renderer.cpp            ← Background/Occluder/Helmet passes
│   └── bindings/
│       └── pybind_wrapper.cpp      ← Python ↔ C++ bridge
├── assets/
│   ├── models/
│   │   ├── helmet.obj              ← [AÑADIR] Malla del casco
│   │   └── face_occluder.obj       ← [AÑADIR] Malla de oclusión
│   └── shaders/
│       ├── background.vert/.frag   ← Full-screen quad con textura cámara
│       ├── occluder.vert/.frag     ← Solo Z-buffer
│       └── lighting.vert/.frag     ← [EQUIPO HUMANO] Blinn-Phong/PBR
├── app_ui/
│   ├── main.py                     ← Orquestador (Qt + pipeline)
│   ├── camera.py                   ← RAII wrapper OpenCV
│   ├── landmark_detector.py        ← MediaPipe + solvePnP
│   └── gui.py                      ← Ventana PyQt6
├── tests/
│   └── test_pose.py                ← Demo Semana 5 (puro Python)
└── requirements.txt
```

---

## Pipeline de Renderizado

```
Frame BGR (OpenCV)
    │
    ├── [Python] MediaPipe FaceMesh → 6 landmarks 2D
    ├── [Python] cv2.solvePnP → rvec, tvec
    ├── [C++]    PoseEstimator::buildModelView(rvec, tvec) → glm::mat4
    │
    └── [C++ OpenGL — Renderer::render()]
         │
         ├── Pasada 1 — Background
         │   └── Full-screen quad con textura BGR de cámara
         │
         ├── Pasada 2 — Occluder (Z-buffer only)
         │   └── face_occluder.obj → glColorMask(false) → solo depth
         │
         └── Pasada 3 — Helmet 3D
              ├── lighting.vert/frag (uniforms: lightDir, lightColor, ambientColor)
              └── [EQUIPO HUMANO] Implementar Blinn-Phong / PBR
```

---

## Responsabilidades del Equipo

### Módulos del equipo de software (este repositorio)
- ✅ `CMakeLists.txt` — Build system completo
- ✅ `Renderer.cpp` — Pipeline OpenGL de 3 pasadas
- ✅ `PoseEstimator.cpp` — Conversión Rodrigues + intrínsecas
- ✅ `Model.cpp` / `Shader.cpp` — Cargadores de activos
- ✅ `pybind_wrapper.cpp` — Puente Python ↔ C++
- ✅ `camera.py` / `landmark_detector.py` — Captura y detección
- ✅ `lighting.vert` — Vertex shader de iluminación

### Módulos del equipo humano (a implementar)
- 🔴 `LightEstimator.cpp` — Estimación fotométrica (gradientes, histogramas)
- 🔴 `lighting.frag` — Modelo de sombreado (Blinn-Phong / PBR)

---

## Dependencias (gestionadas automáticamente por CMake)

| Librería       | Versión | Fuente        | Propósito                         |
|----------------|---------|---------------|-----------------------------------|
| pybind11       | 2.13.6  | FetchContent  | Python ↔ C++ bridge               |
| GLFW           | 3.4     | FetchContent  | Contexto OpenGL + ventana offscreen|
| GLM            | 1.0.1   | FetchContent  | Matemáticas de gráficos            |
| Assimp         | 5.4.3   | FetchContent  | Carga de modelos OBJ               |
| glad2          | latest  | FetchContent  | OpenGL 3.3 Core loader             |
| stb            | latest  | FetchContent  | stb_image para texturas            |

---

## Solución de Problemas

### `ImportError: No module named 'ar_helmet_core'`
El módulo no está compilado o no está en el Python path.
```bash
# Verificar que el .pyd/.so está en la raíz
ls ar_helmet_core*.pyd  # Windows
ls ar_helmet_core*.so   # Linux
# Si no existe, compilar:
cmake --build build --config Release
```

### Error de GLFW al inicializar
En Linux, puede requerir librerías de sistema:
```bash
sudo apt-get install libgl1-mesa-dev libxrandr-dev libxinerama-dev \
                     libxcursor-dev libxi-dev
```

### MediaPipe no detecta landmarks
- Asegurarse de pasar el frame en **RGB** (no BGR) a `detect()`.
- Mejorar iluminación del entorno.
- Reducir `min_detection_confidence` a 0.3.

### FPS < 30
- Cerrar otras aplicaciones que usen la cámara.
- Reducir resolución: `--width 640 --height 480`.
- Verificar que se usa la GPU correcta (variables `CUDA_VISIBLE_DEVICES`).

---

## Licencia

Proyecto académico — CS4016 Computación Gráfica.  
Uso exclusivo para fines educativos.
