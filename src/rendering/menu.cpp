// =============================================================================
// menu.cpp — Implémentation du panneau de boutons, OrbitalSim
//
// Voir menu.h pour la documentation de l'interface.
//
// draw()       : fond sombre via OpenGL (rectangle plein)
// drawImGui()  : boutons avec labels texte via Dear ImGui
// =============================================================================

#include "rendering/menu.h"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>


// =============================================================================
// Menu::draw — fond OpenGL seulement
// =============================================================================

void Menu::draw(DynBuf2D& buf, GLint locColor,
                int splitX, int fbW, int /*fbH*/, int mapTopY) const
{
    if (mapTopY <= 0) return;

    // Fond de la zone menu (ImGui dessinera ses éléments par-dessus)
    draw_2d(buf,
            make_rect(static_cast<float>(splitX), 0.0f,
                      static_cast<float>(fbW),    static_cast<float>(mapTopY)),
            GL_TRIANGLES, locColor,
            { 0.05f, 0.08f, 0.13f, 1.0f });
}


// =============================================================================
// Menu::drawImGui — boutons ImGui avec labels
// =============================================================================

void Menu::drawImGui(int splitX, int fbW, int mapTopY) const
{
    if (mapTopY <= 0) return;

    // ── Fenêtre ancrée — position et taille fixes chaque frame ───────────────
    ImGui::SetNextWindowPos (ImVec2(static_cast<float>(splitX), 0.0f),
                             ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(fbW - splitX),
                                    static_cast<float>(mapTopY)),
                             ImGuiCond_Always);

    // ── Style de fenêtre ──────────────────────────────────────────────────────
    // Couleurs et marges adaptées au thème sombre du projet.
    // Poussés AVANT Begin() → s'appliquent à la fenêtre.
    ImGui::PushStyleColor(ImGuiCol_WindowBg,  ImVec4(0.05f, 0.08f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,    ImVec4(0.22f, 0.40f, 0.58f, 1.0f));
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowPadding, ImVec2(PADDING, PADDING));
    ImGui::PushStyleVar  (ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowBorderSize, 1.0f);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoTitleBar        |
        ImGuiWindowFlags_NoResize          |
        ImGuiWindowFlags_NoMove            |
        ImGuiWindowFlags_NoScrollbar       |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoBringToDisplayFront;

    if (ImGui::Begin("##OrbitalMenu", nullptr, kFlags))
    {
        const float btnW = ImGui::GetContentRegionAvail().x;

        // Espacement entre les boutons
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(0.0f, BUTTON_GAP));

        // ── Définition des boutons ────────────────────────────────────────────
        // Chaque bouton a 3 couleurs : repos / survol / clic.
        // La couleur de base est une version sombre de la teinte d'accent pour
        // rester lisible sur fond noir, et s'illumine au survol/clic.
        struct BtnDef {
            const char* label;
            ImVec4      col;     // état normal
            ImVec4      colHov;  // survol
            ImVec4      colAct;  // clic
        };

        static const BtnDef kBtns[N_BUTTONS] = {
            {   // 0 — Ajouter un satellite (vert)
                "Ajouter un satellite",
                ImVec4(0.08f, 0.28f, 0.14f, 1.0f),
                ImVec4(0.13f, 0.44f, 0.22f, 1.0f),
                ImVec4(0.20f, 0.72f, 0.42f, 1.0f)
            },
            {   // 1 — Ajouter une zone d'atterrissage (cyan)
                "Ajouter une zone d'atterrissage",
                ImVec4(0.06f, 0.22f, 0.30f, 1.0f),
                ImVec4(0.10f, 0.36f, 0.50f, 1.0f),
                ImVec4(0.18f, 0.62f, 0.82f, 1.0f)
            },
            {   // 2 — Réinitialiser (orange)
                "R\xc3\xa9initialiser",
                ImVec4(0.32f, 0.18f, 0.03f, 1.0f),
                ImVec4(0.52f, 0.30f, 0.05f, 1.0f),
                ImVec4(0.92f, 0.52f, 0.10f, 1.0f)
            },
            {   // 3 — Contrôles (violet)
                "Contr\xc3\xb4les",
                ImVec4(0.18f, 0.14f, 0.32f, 1.0f),
                ImVec4(0.30f, 0.24f, 0.52f, 1.0f),
                ImVec4(0.52f, 0.42f, 0.92f, 1.0f)
            },
            {   // 4 — Supprimer objet (rouge)
                "Supprimer objet",
                ImVec4(0.28f, 0.08f, 0.07f, 1.0f),
                ImVec4(0.46f, 0.13f, 0.11f, 1.0f),
                ImVec4(0.82f, 0.22f, 0.18f, 1.0f)
            },
        };

        for (const auto& b : kBtns)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,        b.col);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, b.colHov);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  b.colAct);

            // Le bouton retourne true si cliqué — actions à brancher plus tard
            ImGui::Button(b.label, ImVec2(btnW, BUTTON_H));

            ImGui::PopStyleColor(3);
        }

        ImGui::PopStyleVar();   // ItemSpacing
    }
    ImGui::End();

    ImGui::PopStyleVar  (3);   // WindowBorderSize, FrameRounding, WindowPadding
    ImGui::PopStyleColor(2);   // Border, WindowBg
}
