#include "SystemFunction.h"

#include "Setting.h"
#include "Config.h"
#include "Misc.h"

using namespace std;
using namespace boost::filesystem;


static int CALLBACK FontEnumerationProc(ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *lpntme, DWORD FontType, LPARAM lParam);

static int CALLBACK FontEnumerationProc(ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *lpntme, DWORD FontType, LPARAM lParam)
{
    return 0;
}

SImage* LoadSystemImage(const string & file)
{
    path p = Setting::GetRootDirectory() / SU_DATA_DIR / SU_IMAGE_DIR / ConvertUTF8ToShiftJis(file);
    return SImage::CreateLoadedImageFromFile(p.string());
}

SFont* LoadSystemFont(const std::string & file)
{
    path p = Setting::GetRootDirectory() / SU_DATA_DIR / SU_FONT_DIR / (ConvertUTF8ToShiftJis(file) + ".sif");
    return SFont::CreateLoadedFontFromFile(p.string());
}

void CreateImageFont(const std::string & fileName, const std::string & saveName, int size)
{
    Font::CreateAndSave(fileName, saveName, size, 1024, 1024);
}

void EnumerateInstalledFonts()
{
    //HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\Fonts
    HDC hdc = GetDC(GetMainWindowHandle());
    LOGFONT logfont;
    logfont.lfCharSet = DEFAULT_CHARSET;
    memcpy_s(logfont.lfFaceName, sizeof(logfont.lfFaceName), "", 1);
    logfont.lfPitchAndFamily = 0;
    EnumFontFamiliesEx(hdc, &logfont, (FONTENUMPROC)FontEnumerationProc, (LPARAM)nullptr, 0);
}