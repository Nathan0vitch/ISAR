#pragma once
// =============================================================================
// menu.h — Panneau de contrôle, OrbitalSim
//
// Le menu occupe la zone en haut à droite de la fenêtre, au-dessus du
// planisphère. Sa hauteur est FIXE (HEIGHT pixels) pour rester toujours
// lisible. Il s'élargit horizontalement avec le panneau droit (splitX).
//
// PANNEAUX (machine d'état interne) :
//   MenuPanel::Main          — 5 boutons principaux
//   MenuPanel::AddSatellite  — formulaire "Nouveau satellite"
//
// BOUTONS principaux :
//   0 — Ajouter un satellite
//   1 — Ajouter une zone d'atterrissage
//   2 — Réinitialiser
//   3 — Contrôles
//   4 — Supprimer objet
// =============================================================================

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "rendering/affichage.h"    // DynBuf2D, draw_2d, make_rect
#include "simulation/Satellite.h"  // OrbitalParams, PhysicalParams, Satellite

#include <vector>


// =============================================================================
// MenuPanel — état courant du panneau
// =============================================================================
enum class MenuPanel { Main, AddSatellite };


// =============================================================================
// Menu
// =============================================================================
struct Menu
{
    // ── Dimensions constantes des boutons principaux (pixels) ─────────────────
    static constexpr float BUTTON_H   = 34.0f;
    static constexpr float BUTTON_GAP =  6.0f;
    static constexpr float PADDING    = 10.0f;
    static constexpr int   N_BUTTONS  =  5;

    // Hauteur minimale du menu principal
    static constexpr float HEIGHT =
        2.0f * PADDING
        + N_BUTTONS  * BUTTON_H
        + (N_BUTTONS - 1) * BUTTON_GAP;

    // ── État de la machine d'état ─────────────────────────────────────────────
    MenuPanel panel = MenuPanel::Main;

    // Paramètres du satellite en cours de saisie
    OrbitalParams  pendingOrbit;
    PhysicalParams pendingPhysics;

    // ── Dessin OpenGL (fond) ──────────────────────────────────────────────────
    void draw(DynBuf2D& buf, GLint locColor,
              int splitX, int fbW, int fbH, int mapTopY) const;

    // ── Dessin ImGui (boutons + formulaire) ───────────────────────────────────
    //
    // Renvoie true quand l'utilisateur valide "Ajouter" :
    //   → pendingOrbit et pendingPhysics contiennent le satellite à créer.
    //
    // Non-const car la machine d'état et les champs du formulaire sont mutables.
    bool drawImGui(int splitX, int fbW, int mapTopY);
};
