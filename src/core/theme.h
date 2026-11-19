#ifndef CORE_THEME_H
#define CORE_THEME_H

#include <QString>

class QApplication;

namespace Theme {

// Paleta corporativa descansada para jornadas prolongadas
inline constexpr const char *kPrimaryDark    = "#1E3A5F";
inline constexpr const char *kPrimaryHover   = "#274C77";
inline constexpr const char *kAccentTeal     = "#2B7A78";
inline constexpr const char *kAccentHover    = "#226361";
inline constexpr const char *kCanvasBg       = "#F4F7FA";
inline constexpr const char *kSurfaceCard    = "#FFFFFF";
inline constexpr const char *kBorderSoft     = "#D9E2EC";
inline constexpr const char *kTextPrimary    = "#1F2933";
inline constexpr const char *kTextSecondary  = "#52606D";
inline constexpr const char *kDangerRed      = "#C53030";
inline constexpr const char *kWarningAmber   = "#B7791F";
inline constexpr const char *kSuccessGreen   = "#2E7D32";

void apply(QApplication &app);
QString styleSheet();

} // namespace Theme

#endif // CORE_THEME_H
