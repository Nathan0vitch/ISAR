# OrbitalSim — Rentrée & trajectoire orbitale (C++17 / OpenGL 3.3)

Simulation 3D d'une orbite basse terrestre, d'une impulsion de déorbitation et d'une rentrée atmosphérique avec rendu temps réel.

---

## Architecture

```
OrbitalSim/
├── CMakeLists.txt
├── include/
│   ├── simulation/
│   │   ├── Physics.h          — constantes, StateVector, OrbitalElements, Telemetry
│   │   ├── Atmosphere.h       — modèle exponentiel (densité, chaleur, Mach)
│   │   ├── Spacecraft.h       — intégrateur RK4, phases orbite / rentrée / atterrissage
│   │   └── OrbitalElements.h  — conversion éléments kepleriens ↔ vecteur d'état ECI
│   ├── rendering/
│   │   ├── Renderer.h         — rendu OpenGL (Terre, trajet, marqueur)
│   │   ├── Shader.h           — compilation GLSL, uniformes
│   │   ├── Camera.h           — caméra arcball (orbite, zoom, pan)
│   │   └── Mesh.h             — géométrie UV-sphère
│   └── core/
│       ├── Window.h           — fenêtre GLFW + callbacks
│       ├── InputHandler.h     — clavier / souris → caméra & simulation
│       └── Timer.h            — deltaTime frame
├── src/                       — implémentations .cpp
├── shaders/                   — (vide — shaders embarqués dans le source)
└── vendor/glad/               — GLAD OpenGL loader (voir ci-dessous)
```

---

## Physique implémentée

| Module | Détails |
|--------|---------|
| **Gravité** | Corps à deux corps, μ_Terre = 3.986 × 10¹⁴ m³/s² |
| **Intégration** | Runge-Kutta 4 — pas 10 s (orbite) / 0,5 s (rentrée) |
| **Atmosphère** | Modèle exponentiel ρ = ρ₀·exp(−h/H), H = 8 500 m |
| **Aérodynamique** | Traînée + portance (Cd, Cl, A_ref configurable) |
| **Flux thermique** | Detra–Kemp–Riddell simplifié q ~ √(ρ/R_n)·v³ |
| **Éléments kepleriens** | Conversion bidirectionnelle ECI ↔ (a, e, i, Ω, ω, ν) |

---

## Scénario par défaut

Orbite circulaire à **200 km** (inclinaison 28,5°), impulsion rétrograde de **−120 m/s**,
rentrée ballastique capsule type Crew Dragon (m = 9 500 kg, Cd = 1,28, A = 12 m²).

---

## Dépendances

| Bibliothèque | Version min | Installation |
|-------------|-------------|--------------|
| CMake       | 3.16        | https://cmake.org |
| GLFW        | 3.3         | `sudo apt install libglfw3-dev` / `brew install glfw` |
| GLM         | 0.9.9       | `sudo apt install libglm-dev` / auto-FetchContent |
| GLAD        | —           | Voir ci-dessous |

### Obtenir GLAD

1. Aller sur https://glad.dav1d.de/
2. Language: **C/C++**, Spec: **OpenGL**, Profile: **Core**, Version: **3.3**, ☑ *Generate a loader*
3. Copier dans `vendor/glad/` :
   ```
   include/glad/glad.h
   include/KHR/khrplatform.h
   src/glad.c
   ```

---

## Build

```bash
# Cloner / entrer dans le dossier
cd OrbitalSim

# Configurer
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Compiler
cmake --build build --parallel

# Lancer
./build/OrbitalSim
```

---

## Contrôles

| Touche / Geste | Action |
|----------------|--------|
| **LMB drag** | Faire tourner la caméra |
| **RMB drag** | Déplacer la caméra (pan) |
| **Molette** | Zoom |
| **Espace** | Pause / Reprise |
| **N** | Avancer d'un pas (mode pause) |
| **+** / **−** | Vitesse × 2 / ÷ 2 |
| **R** | Réinitialiser la caméra |
| **ESC** / **Q** | Quitter |

---

## Étapes suivantes suggérées

- [ ] Modèle atmosphérique NRLMSISE-00 (plus précis)
- [ ] Perturbations J2 (aplatissement terrestre)
- [ ] HUD OpenGL / ImGui : altitude, vitesse, Mach, G, flux thermique
- [ ] Texture Terre (NASA Blue Marble)
- [ ] Export CSV de la télémétrie
- [ ] Scénario configurable via fichier JSON
