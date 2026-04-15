#pragma once
// =============================================================================
// menu.h — Panneau de boutons du planisphère, OrbitalSim
//
// Le menu occupe la zone en haut à droite de la fenêtre, au-dessus du
// planisphère. Sa hauteur est FIXE (HEIGHT pixels) pour rester toujours
// lisible. Il s'élargit horizontalement avec le panneau droit (splitX).
//
// Il est rendu EN DESSOUS du planisphère : quand mapTopY < HEIGHT, le
// planisphère le recouvre partiellement ou totalement.
//
// Les boutons sont pour l'instant des rectangles colorés (les labels texte
// seront ajoutés lors de l'intégration ImGui).
//
// BOUTONS (dans l'ordre, de haut en bas) :
//   0 — Ajouter un satellite          (accent vert)
//   1 — Ajouter une zone d'att.        (accent cyan)
//   2 — Réinitialiser                  (accent orange)
//   3 — Contrôles                      (accent violet)
//   4 — Supprimer objet                (accent rouge)
// =============================================================================

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "rendering/affichage.h"   // DynBuf2D, draw_2d, make_rect


// =============================================================================
// Menu
// =============================================================================

struct Menu
{
    // ── Dimensions (constantes, en pixels) ───────────────────────────────────
    static constexpr float BUTTON_H   = 34.0f;   // Hauteur d'un bouton
    static constexpr float BUTTON_GAP =  6.0f;   // Espacement vertical entre boutons
    static constexpr float PADDING    = 10.0f;   // Marge (haut, bas, gauche, droite)
    static constexpr int   N_BUTTONS  =  5;      // Nombre de boutons

    // Hauteur totale du menu (calculée une fois à la compilation)
    static constexpr float HEIGHT =
        2.0f * PADDING
        + N_BUTTONS  * BUTTON_H
        + (N_BUTTONS - 1) * BUTTON_GAP;

    // ── Dessin ────────────────────────────────────────────────────────────────
    //
    // Rend le menu dans la zone [splitX, 0] → [fbW, mapTopY] en coordonnées
    // pixels-écran (y=0 en haut).
    //
    // Le shader 2D doit déjà être actif avec une projection ortho couvrant
    // toute la fenêtre (0, fbW, fbH, 0).
    //
    // Si mapTopY <= 0 la fonction retourne immédiatement (rien à dessiner).
    void draw(DynBuf2D& buf, GLint locColor,
              int splitX, int fbW, int fbH, int mapTopY) const;
};
