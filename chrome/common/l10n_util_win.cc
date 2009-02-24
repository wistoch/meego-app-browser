#include "chrome/common/l10n_util.h"
#include "chrome/common/l10n_util_win.h"

namespace l10n_util {

int GetExtendedStyles() {
  return GetTextDirection() == LEFT_TO_RIGHT ? 0 :
      WS_EX_LAYOUTRTL | WS_EX_RTLREADING;
}

int GetExtendedTooltipStyles() {
  return GetTextDirection() == LEFT_TO_RIGHT ? 0 : WS_EX_LAYOUTRTL;
}

void HWNDSetRTLLayout(HWND hwnd) {
  DWORD ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);

  // We don't have to do anything if the style is already set for the HWND.
  if (!(ex_style & WS_EX_LAYOUTRTL)) {
    ex_style |= WS_EX_LAYOUTRTL;
    ::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);

    // Right-to-left layout changes are not applied to the window immediately
    // so we should make sure a WM_PAINT is sent to the window by invalidating
    // the entire window rect.
    ::InvalidateRect(hwnd, NULL, true);
  }
}

}  // namespace l10n_util

