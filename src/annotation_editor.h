#pragma once

#ifdef _WIN32
#include <windows.h>
#include <objidl.h>

namespace webcom {

// Opens a lightweight native screenshot editor. The supplied stream must contain
// a PNG image. The editor uses standard Win32 buttons for Pen, Rectangle, Undo,
// Copy and Save so the toolbar is keyboard/screen-reader accessible.
bool ShowScreenshotAnnotationEditor(HWND owner, HINSTANCE instance, IStream* png_stream, UINT dpi);

} // namespace webcom
#endif
