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
// BOUTONS (dans l'ordre, de haut en bas) :
//   0 — Ajouter un satellite
//   1 — Ajouter une zone d'atterrissage
//   2 — Réinitialiser
//   3 — Contrôles
//   4 — Supprimer objet
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

    // ── Dessin OpenGL (fond sombre) ───────────────────────────────────────────
    //
    // Rend uniquement le fond de la zone menu via le shader 2D.
    // Les boutons sont rendus par drawImGui() — appelé séparément.
    //
    // Si mapTopY <= 0 la fonction retourne immédiatement.
    void draw(DynBuf2D& buf, GLint locColor,
              int splitX, int fbW, int fbH, int mapTopY) const;

    // ── Dessin ImGui (boutons avec labels) ───────────────────────────────────
    //
    // Crée une fenêtre ImGui ancrée en haut du panneau droit.
    // Doit être appelé entre ImGui::NewFrame() et ImGui::Render().
    //
    // Paramètres en pixels-écran (y=0 en haut, convention GLFW/ImGui) :
    //   splitX  : bord gauche du panneau droit
    //   fbW     : largeur totale du framebuffer
    //   mapTopY : hauteur disponible pour le menu (= Y où commence le planisphère)
    //
    // Si mapTopY <= 0, la fenêtre n'est pas affichée.
    void drawImGui(int splitX, int fbW, int mapTopY) const;
};
