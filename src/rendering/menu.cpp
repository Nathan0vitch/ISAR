// =============================================================================
// menu.cpp — Panneau de contrôle, OrbitalSim
//
// draw()       : fond OpenGL (rectangle sombre derrière ImGui)
// drawImGui()  : machine d'état → menu principal OU formulaire satellite
// =============================================================================

#include "rendering/menu.h"

#include <imgui.h>
#include <cstdio>   // snprintf


// =============================================================================
// Helpers ImGui internes
// =============================================================================

// Ligne de paramètre : label à gauche, widget à droite (table 2 colonnes active)
static void tableLabel(const char* txt)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(txt);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
}

// Ligne read-only : label + valeur texte formatée
static void tableReadOnly(const char* label, const char* value)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.75f, 0.95f, 1.0f));
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
}


// =============================================================================
// Menu::draw — fond OpenGL
// =============================================================================
void Menu::draw(DynBuf2D& buf, GLint locColor,
                int splitX, int fbW, int /*fbH*/, int mapTopY) const
{
    if (mapTopY <= 0) return;
    draw_2d(buf,
            make_rect(static_cast<float>(splitX), 0.0f,
                      static_cast<float>(fbW),    static_cast<float>(mapTopY)),
            GL_TRIANGLES, locColor,
            { 0.05f, 0.08f, 0.13f, 1.0f });
}


// =============================================================================
// Menu::drawImGui
// =============================================================================
bool Menu::drawImGui(int splitX, int fbW, int mapTopY)
{
    if (mapTopY <= 0) return false;

    // ── Fenêtre ancrée ────────────────────────────────────────────────────────
    ImGui::SetNextWindowPos (ImVec2(static_cast<float>(splitX), 0.0f),
                             ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(fbW - splitX),
                                    static_cast<float>(mapTopY)),
                             ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.08f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.22f, 0.40f, 0.58f, 1.0f));
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowPadding,    ImVec2(PADDING, PADDING));
    ImGui::PushStyleVar  (ImGuiStyleVar_FrameRounding,    3.0f);
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowBorderSize, 1.0f);

    const ImGuiWindowFlags kWinFlags =
        ImGuiWindowFlags_NoTitleBar      |
        ImGuiWindowFlags_NoResize        |
        ImGuiWindowFlags_NoMove          |
        ImGuiWindowFlags_NoSavedSettings;

    bool addRequested = false;

    if (ImGui::Begin("##OrbitalMenu", nullptr, kWinFlags))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, BUTTON_GAP));

        if (panel == MenuPanel::Main)
            drawMainPanel();
        else if (panel == MenuPanel::AddSatellite)
            addRequested = drawSatelliteForm();

        ImGui::PopStyleVar();  // ItemSpacing
    }
    ImGui::End();

    ImGui::PopStyleVar  (3);
    ImGui::PopStyleColor(2);

    return addRequested;
}


// =============================================================================
// drawMainPanel — 5 boutons principaux
// =============================================================================
void Menu::drawMainPanel()
{
    const float btnW = ImGui::GetContentRegionAvail().x;

    struct BtnDef { const char* label; ImVec4 col, colHov, colAct; };
    static const BtnDef kBtns[N_BUTTONS] = {
        { "Ajouter un satellite",
          {0.08f,0.28f,0.14f,1}, {0.13f,0.44f,0.22f,1}, {0.20f,0.72f,0.42f,1} },
        { "Ajouter une zone d'atterrissage",
          {0.06f,0.22f,0.30f,1}, {0.10f,0.36f,0.50f,1}, {0.18f,0.62f,0.82f,1} },
        { "R\xc3\xa9initialiser",
          {0.32f,0.18f,0.03f,1}, {0.52f,0.30f,0.05f,1}, {0.92f,0.52f,0.10f,1} },
        { "Contr\xc3\xb4les",
          {0.18f,0.14f,0.32f,1}, {0.30f,0.24f,0.52f,1}, {0.52f,0.42f,0.92f,1} },
        { "Supprimer objet",
          {0.28f,0.08f,0.07f,1}, {0.46f,0.13f,0.11f,1}, {0.82f,0.22f,0.18f,1} },
    };

    for (int i = 0; i < N_BUTTONS; ++i)
    {
        const auto& b = kBtns[i];
        ImGui::PushStyleColor(ImGuiCol_Button,        b.col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, b.colHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  b.colAct);

        if (ImGui::Button(b.label, ImVec2(btnW, BUTTON_H)))
        {
            if (i == 0) {   // "Ajouter un satellite"
                pendingOrbit  = OrbitalParams{};   // reset aux valeurs par défaut
                pendingPhysics= PhysicalParams{};
                pendingOrbit.recalculate();
                panel = MenuPanel::AddSatellite;
            }
            // TODO : autres boutons
        }

        ImGui::PopStyleColor(3);
    }
}


// =============================================================================
// drawSatelliteForm — formulaire "Nouveau satellite"
// Retourne true quand l'utilisateur clique sur "Ajouter".
// =============================================================================
bool Menu::drawSatelliteForm()
{
    // Recalcule les champs dérivés chaque frame (coût négligeable)
    pendingOrbit.recalculate();

    bool addClicked = false;

    // ── En-tête ───────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f,0.25f,0.35f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f,0.38f,0.52f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.30f,0.52f,0.72f,1));
    if (ImGui::Button("\xe2\x86\x90", ImVec2(28, 0)))   // ← (UTF-8)
        panel = MenuPanel::Main;
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.88f,0.92f,0.98f,1), "Nouveau satellite");

    ImGui::Separator();

    // ── Table commune : label (55%) | valeur (45%) ────────────────────────────
    constexpr ImGuiTableFlags kTbl =
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_BordersInnerH     |
        ImGuiTableFlags_PadOuterX;

    // ═════ SECTION ORBITE ════════════════════════════════════════════════════
    ImGui::TextColored(ImVec4(0.50f,0.80f,1.0f,1), "ORBITE");

    if (ImGui::BeginTable("##orb", 2, kTbl))
    {
        ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthStretch, 0.58f);
        ImGui::TableSetupColumn("val", ImGuiTableColumnFlags_WidthStretch, 0.42f);

        // -- Saisies (step=0 : champ texte pur, cliquer et taper la valeur) --
        tableLabel("P\xc3\xa9rig\xc3\xa9\x65 h_p [km]");           // Périgée
        ImGui::InputFloat("##hp",   &pendingOrbit.h_perigee,   0.f, 0.f, "%.1f");

        tableLabel("Apog\xc3\xa9\x65 h_a [km]");                    // Apogée
        ImGui::InputFloat("##ha",   &pendingOrbit.h_apogee,    0.f, 0.f, "%.1f");

        tableLabel("Inclinaison i [\xc2\xb0]");                     // °
        ImGui::InputFloat("##inc",  &pendingOrbit.inclination, 0.f, 0.f, "%.2f");

        tableLabel("RAAN \xce\xa9 [\xc2\xb0]");                     // Ω
        ImGui::InputFloat("##raan", &pendingOrbit.raan,        0.f, 0.f, "%.2f");

        tableLabel("Arg. p\xc3\xa9rig\xc3\xa9\x65 \xcf\x89 [\xc2\xb0]"); // ω
        ImGui::InputFloat("##argp", &pendingOrbit.arg_perigee, 0.f, 0.f, "%.2f");

        tableLabel("Anomalie vraie \xce\xbd [\xc2\xb0]");           // ν
        ImGui::InputFloat("##nu",   &pendingOrbit.true_anomaly, 0.f, 0.f, "%.2f");

        // -- Calculés --
        ImGui::TableNextRow(); // ligne de séparation visuelle
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(ImVec4(0.45f,0.55f,0.65f,1), "-- calcul\xc3\xa9s --");

        char buf[48];

        snprintf(buf, sizeof(buf), "%.1f km", pendingOrbit.semi_major_axis);
        tableReadOnly("Demi-grand axe a", buf);

        snprintf(buf, sizeof(buf), "%.6f", pendingOrbit.eccentricity);
        tableReadOnly("Excentricit\xc3\xa9 e", buf);

        {
            float T  = pendingOrbit.period_s;
            int   Tm = static_cast<int>(T / 60.0f);
            snprintf(buf, sizeof(buf), "%.0f s  (%d min)", T, Tm);
        }
        tableReadOnly("P\xc3\xa9riode T", buf);

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // ═════ SECTION SATELLITE ═════════════════════════════════════════════════
    ImGui::TextColored(ImVec4(0.50f,0.80f,1.0f,1), "SATELLITE");

    if (ImGui::BeginTable("##sat", 2, kTbl))
    {
        ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthStretch, 0.58f);
        ImGui::TableSetupColumn("val", ImGuiTableColumnFlags_WidthStretch, 0.42f);

        tableLabel("Masse m [kg]");
        ImGui::InputFloat("##mass", &pendingPhysics.mass,              0.f, 0.f, "%.1f");

        tableLabel("Surface A [m\xc2\xb2]");                        // m²
        ImGui::InputFloat("##area", &pendingPhysics.area,             0.f, 0.f, "%.4f");

        tableLabel("Coeff. tra\xc3\xae\x6e\xc3\xa9\x65 Cd");        // traînée
        ImGui::InputFloat("##cd",   &pendingPhysics.cd,               0.f, 0.f, "%.2f");

        tableLabel("Coeff. balistique \xce\xb2 [kg/m\xc2\xb2]");   // β
        ImGui::InputFloat("##beta", &pendingPhysics.ballistic_coeff,  0.f, 0.f, "%.1f");

        tableLabel("Finesse parachute L/D");
        ImGui::InputFloat("##fin",  &pendingPhysics.parachute_finesse, 0.f, 0.f, "%.1f");

        ImGui::EndTable();
    }

    // ── Boutons Annuler / Ajouter ─────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float btnW = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.25f,0.15f,0.15f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.45f,0.20f,0.18f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.70f,0.22f,0.18f,1));
    if (ImGui::Button("Annuler", ImVec2(btnW, BUTTON_H)))
        panel = MenuPanel::Main;
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0, 8.0f);

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f,0.30f,0.15f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f,0.48f,0.24f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.22f,0.70f,0.38f,1));
    if (ImGui::Button("Ajouter", ImVec2(btnW, BUTTON_H)))
    {
        panel      = MenuPanel::Main;
        addClicked = true;
    }
    ImGui::PopStyleColor(3);

    return addClicked;
}
