// =============================================================================
// menu.cpp — Implémentation du panneau de boutons, OrbitalSim
//
// Voir menu.h pour la documentation de l'interface.
//
// Rendu actuel : rectangles colorés (labels texte → ImGui, étape suivante).
// Chaque bouton a :
//   • Un fond sombre uniforme
//   • Une barre d'accent colorée sur le bord gauche (4 px)
//   • Un cadre fin (GL_LINES)
// =============================================================================

#include "rendering/menu.h"

#include <glm/gtc/type_ptr.hpp>


// ── Couleurs d'accent par bouton ──────────────────────────────────────────────
// (dans l'ordre déclaré dans menu.h)
static const glm::vec4 ACCENT_COLORS[Menu::N_BUTTONS] = {
    { 0.20f, 0.72f, 0.42f, 1.0f },   // 0 — Ajouter un satellite     (vert)
    { 0.18f, 0.62f, 0.82f, 1.0f },   // 1 — Zone d'atterrissage      (cyan)
    { 0.92f, 0.52f, 0.10f, 1.0f },   // 2 — Réinitialiser             (orange)
    { 0.52f, 0.42f, 0.92f, 1.0f },   // 3 — Contrôles                (violet)
    { 0.82f, 0.22f, 0.18f, 1.0f },   // 4 — Supprimer objet           (rouge)
};


// =============================================================================
// Menu::draw
// =============================================================================

void Menu::draw(DynBuf2D& buf, GLint locColor,
                int splitX, int fbW, int /*fbH*/, int mapTopY) const
{
    if (mapTopY <= 0) return;

    const float x0 = static_cast<float>(splitX);
    const float x1 = static_cast<float>(fbW);

    // ── Fond général du menu ──────────────────────────────────────────────────
    draw_2d(buf, make_rect(x0, 0.0f, x1, static_cast<float>(mapTopY)),
            GL_TRIANGLES, locColor, { 0.05f, 0.08f, 0.13f, 1.0f });

    // ── Titre / séparation haute (barre fine en haut du menu) ─────────────────
    // Bande colorée de 2 px en haut pour délimiter visuellement le menu.
    draw_2d(buf, make_rect(x0, 0.0f, x1, 2.0f),
            GL_TRIANGLES, locColor, { 0.30f, 0.55f, 0.85f, 1.0f });

    // ── Boutons ───────────────────────────────────────────────────────────────
    constexpr float ACCENT_W = 4.0f;   // largeur de la barre d'accent gauche

    const float btnX0 = x0 + PADDING;
    const float btnX1 = x1 - PADDING;

    for (int i = 0; i < N_BUTTONS; ++i)
    {
        const float by0 = PADDING + static_cast<float>(i) * (BUTTON_H + BUTTON_GAP);
        const float by1 = by0 + BUTTON_H;

        // Fond du bouton (légèrement plus clair que le fond du menu)
        draw_2d(buf, make_rect(btnX0, by0, btnX1, by1),
                GL_TRIANGLES, locColor, { 0.10f, 0.15f, 0.22f, 1.0f });

        // Barre d'accent gauche (couleur distinctive du bouton)
        draw_2d(buf, make_rect(btnX0, by0, btnX0 + ACCENT_W, by1),
                GL_TRIANGLES, locColor, ACCENT_COLORS[i]);

        // Cadre du bouton (4 segments GL_LINES, couleur neutre)
        const std::vector<float> border = {
            btnX0, by0,   btnX1, by0,   // arête haute
            btnX1, by0,   btnX1, by1,   // arête droite
            btnX1, by1,   btnX0, by1,   // arête basse
            btnX0, by1,   btnX0, by0    // arête gauche
        };
        draw_2d(buf, border, GL_LINES, locColor,
                { 0.22f, 0.38f, 0.55f, 1.0f });
    }
}
