// Pozitron Koruma.cpp : Uygulamanın giriş noktasını tanımlar.
//

#include "framework.h"
#include "Pozitron Koruma.h"
#include "resource.h"
#include <shellapi.h>
#include <string>
#include <vector>
#include <winhttp.h>
#include <iphlpapi.h>  // GetAdaptersInfo için
#include <wininet.h>
#pragma comment(lib, "iphlpapi.lib")  // IP Helper kütüphanesi
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "winhttp.lib")

#define MAX_LOADSTRING 100

// CryptoLens bilgileri (kendi bilgilerinizle değiştirin)
#define CRYPTOLENS_PRODUCT_ID 32201  // CryptoLens'teki ürün ID'niz
#define CRYPTOLENS_ACCESS_TOKEN L"WyIxMTc2NTQ2MjYiLCJwOVI1S1oxT29SRXN5OUFINm5XMXd3eENmQXJBYzd2cXczcDBtdzhRIl0"  // CryptoLens access token'ınız

// Feature tanımları
enum PozitronFeatures {
    FEATURE_TEMEL_KULLANIM = 1,        // Temel Kullanım (Bit 0: 2^0 = 1)
    FEATURE_DENEME = 2,                 // Deneme (Bit 1: 2^1 = 2)
    FEATURE_PANEL_KULLANIM = 3,         // Panel Kullanım (Bit 2: 2^2 = 4)
    FEATURE_GELISMIS_PANEL = 4,         // Gelişmiş Panel (Bit 3: 2^3 = 8)
    FEATURE_POZITRON = 5,               // Pozitron (Bit 4: 2^4 = 16)
    FEATURE_AI_DESTEKLI = 6             // AI Destekli Kullanım (Bit 5: 2^5 = 32)
};

// Aktivasyon sonucu yapısı
struct ActivationResult {
    bool success;
    std::wstring errorMessage;
    std::vector<int> enabledFeatures;
    std::wstring licensee;
    std::wstring expiryDate;
    int maxMachines;
    int activatedMachines;

    // Constructor
    ActivationResult() : success(false), maxMachines(0), activatedMachines(0) {}
};

// Genel Değişkenler:
HINSTANCE hInst;                                // geçerli örnek
WCHAR szTitle[MAX_LOADSTRING];                  // Başlık çubuğu metni
WCHAR szWindowClass[MAX_LOADSTRING];            // ana pencere sınıfı adı
bool g_bActivated = false;                      // Aktivasyon durumu
bool g_bLoggedIn = false;                       // Giriş durumu
std::wstring g_strUsername = L"";                // Kullanıcı adı
std::vector<int> g_EnabledFeatures;              // Aktif feature'lar
HWND g_hWnd = NULL;                              // Ana pencere handle'ı

// Bitmap değişkenleri
HBITMAP g_hLoginBitmap = NULL;                   // Login/Register olmayanlara gösterilecek fotoğraf

// Fonksiyon prototipleri
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    LoginDialogProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    RegisterDialogProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    ActivationDialogProc(HWND, UINT, WPARAM, LPARAM);
void                DrawCenterBitmap(HWND hWnd, HDC hdc);
bool                CheckInternetConnection();
std::string         GetMachineCode();
bool                CheckActivationWithCryptoLens(const std::wstring& activationCode, ActivationResult& result);
std::vector<int>    ParseFeatureBits(int featureBits);
const wchar_t* GetFeatureName(int featureId);
std::wstring        GetUserPackageLevel(const std::vector<int>& enabledFeatures);
bool                IsFeatureEnabled(int featureId, const std::vector<int>& enabledFeatures);
void                UpdateMenuBasedOnFeatures();
void                ShowActivationSuccessMessage(HWND hWnd, const ActivationResult& result);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Genel dizeleri başlat
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_POZITRONKORUMA, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Uygulama başlatması gerçekleştir:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_POZITRONKORUMA));

    MSG msg;

    // Ana ileti döngüsü:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

//
//  İŞLEV: MyRegisterClass()
//
//  AMAÇ: Pencere sınıfını kaydeder.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_POZITRONKORUMA));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_POZITRONKORUMA);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   İŞLEV: InitInstance(HINSTANCE, int)
//
//   AMAÇ: Örnek tanıtıcısını kaydeder ve ana pencereyi oluşturur
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Örnek tanıtıcısını genel değişkenimizde depola

    // Bitmap'i yükle (login/register olmayanlar için)
    g_hLoginBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_LOGIN_BITMAP));

    // Eğer IDB_LOGIN_BITMAP yoksa varsayılan bir bitmap yüklemeyi dene
    if (g_hLoginBitmap == NULL)
    {
        g_hLoginBitmap = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_REGISTER_BITMAP));
    }

    g_hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 800, 600, nullptr, nullptr, hInstance, nullptr);

    if (!g_hWnd)
    {
        return FALSE;
    }

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    return TRUE;
}

//
//  İŞLEV: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  AMAÇ: Ana pencere için iletileri işler.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Menü seçimlerini ayrıştır:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;

        case IDC_VISIT_WEBSITE:
            ShellExecute(NULL, L"open", L"https://guns.lol/pozitron", NULL, NULL, SW_SHOW);
            break;

        case ID_YARD32773:
            MessageBox(hWnd, L"Log menüsü tıklandı", L"Bilgi", MB_OK);
            break;

        case ID_ADMIN_LOGIN:
            if (DialogBox(hInst, MAKEINTRESOURCE(IDD_LOGIN_DIALOG), hWnd, LoginDialogProc) == IDOK)
            {
                InvalidateRect(hWnd, NULL, TRUE);
            }
            break;

        case ID_ADMIN_REGISTER:
            if (!g_bActivated)
            {
                MessageBox(hWnd, L"Kayıt olmak için önce aktivasyon yapmalısınız!", L"Uyarı", MB_OK | MB_ICONWARNING);

                INT_PTR result = DialogBox(hInst, MAKEINTRESOURCE(IDD_ACTIVATION_DIALOG), hWnd, ActivationDialogProc);

                if (result == IDOK)
                {
                    if (DialogBox(hInst, MAKEINTRESOURCE(IDD_REGISTER_DIALOG), hWnd, RegisterDialogProc) == IDOK)
                    {
                        InvalidateRect(hWnd, NULL, TRUE);
                    }
                }
            }
            else
            {
                if (DialogBox(hInst, MAKEINTRESOURCE(IDD_REGISTER_DIALOG), hWnd, RegisterDialogProc) == IDOK)
                {
                    InvalidateRect(hWnd, NULL, TRUE);
                }
            }
            break;

        case ID_ADMIN_ACTIVATION:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ACTIVATION_DIALOG), hWnd, ActivationDialogProc);
            break;

        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        if (!g_bLoggedIn)
        {
            DrawCenterBitmap(hWnd, hdc);
        }
        else
        {
            RECT rect;
            GetClientRect(hWnd, &rect);

            std::wstring welcomeMsg = L"Hoşgeldiniz, ";
            welcomeMsg += g_strUsername;
            welcomeMsg += L"!";

            DrawText(hdc, welcomeMsg.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        EndPaint(hWnd, &ps);
    }
    break;

    case WM_SIZE:
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_DESTROY:
        if (g_hLoginBitmap)
        {
            DeleteObject(g_hLoginBitmap);
        }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Pencere ortasına bitmap çizme fonksiyonu
void DrawCenterBitmap(HWND hWnd, HDC hdc)
{
    if (g_hLoginBitmap == NULL)
        return;

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;

    BITMAP bm;
    GetObject(g_hLoginBitmap, sizeof(BITMAP), &bm);
    int bmWidth = bm.bmWidth;
    int bmHeight = bm.bmHeight;

    int x = (clientWidth - bmWidth) / 2;
    int y = (clientHeight - bmHeight) / 2;

    HDC hdcMem = CreateCompatibleDC(hdc);
    if (hdcMem)
    {
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, g_hLoginBitmap);
        BitBlt(hdc, x, y, bmWidth, bmHeight, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hbmOld);
        DeleteDC(hdcMem);
    }
}

// Hakkında kutusu için ileti işleyicisi
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }

        if (LOWORD(wParam) == IDC_VISIT_WEBSITE)
        {
            ShellExecute(NULL, L"open", L"https://guns.lol/pozitron", NULL, NULL, SW_SHOW);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// Login Dialog İşleyicisi
INT_PTR CALLBACK LoginDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            WCHAR username[100], password[100];
            GetDlgItemText(hDlg, IDC_LOGIN_USERNAME, username, 100);
            GetDlgItemText(hDlg, IDC_LOGIN_PASSWORD, password, 100);

            if (wcslen(username) > 0 && wcslen(password) > 0)
            {
                g_bLoggedIn = true;
                g_strUsername = username;
                MessageBox(hDlg, L"Giriş başarılı!", L"Başarılı", MB_OK);
                EndDialog(hDlg, IDOK);
            }
            else
            {
                MessageBox(hDlg, L"Kullanıcı adı ve şifre boş olamaz!", L"Hata", MB_OK | MB_ICONERROR);
            }
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// Register Dialog İşleyicisi
INT_PTR CALLBACK RegisterDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            WCHAR username[100], email[100], password[100], confirmPassword[100];
            GetDlgItemText(hDlg, IDC_REGISTER_USERNAME, username, 100);
            GetDlgItemText(hDlg, IDC_REGISTER_EMAIL, email, 100);
            GetDlgItemText(hDlg, IDC_REGISTER_PASSWORD, password, 100);
            GetDlgItemText(hDlg, IDC_REGISTER_CONFIRM_PASSWORD, confirmPassword, 100);

            if (wcslen(username) == 0 || wcslen(email) == 0 || wcslen(password) == 0)
            {
                MessageBox(hDlg, L"Lütfen tüm alanları doldurun!", L"Hata", MB_OK | MB_ICONERROR);
                return (INT_PTR)TRUE;
            }

            if (wcscmp(password, confirmPassword) != 0)
            {
                MessageBox(hDlg, L"Şifreler eşleşmiyor!", L"Hata", MB_OK | MB_ICONERROR);
                return (INT_PTR)TRUE;
            }

            g_bLoggedIn = true;
            g_strUsername = username;
            MessageBox(hDlg, L"Kayıt başarılı! Otomatik olarak giriş yapıldı.", L"Başarılı", MB_OK);
            EndDialog(hDlg, IDOK);
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// İnternet bağlantısı kontrolü (BU FONKSİYON BURADA OLMALI)
bool CheckInternetConnection()
{
    BOOL bState;
    DWORD dwFlags;

    bState = InternetGetConnectedState(&dwFlags, 0);

    if (!bState) {
        if (dwFlags & INTERNET_CONNECTION_OFFLINE) {
            return false;
        }
        if (dwFlags & INTERNET_CONNECTION_CONFIGURED) {
            HINTERNET hNet = InternetOpen(L"PozitronChecker",
                INTERNET_OPEN_TYPE_PRECONFIG,
                NULL, NULL, 0);
            if (hNet) {
                HINTERNET hUrl = InternetOpenUrl(hNet,
                    L"http://www.msftconnecttest.com/connecttest.txt",
                    NULL, 0,
                    INTERNET_FLAG_NO_CACHE_WRITE |
                    INTERNET_FLAG_PRAGMA_NOCACHE |
                    INTERNET_FLAG_RELOAD, 0);
                if (hUrl) {
                    InternetCloseHandle(hUrl);
                    InternetCloseHandle(hNet);
                    return true;
                }
                InternetCloseHandle(hNet);
            }
            return false;
        }
    }

    return bState == TRUE;
}

// Makine kodu oluşturma (CryptoLens için)
std::string GetMachineCode()
{
    char machineCode[256] = "";
    char volumeSerial[32] = "";
    char computerName[64] = "";
    char macAddress[18] = "";

    // Volume Serial Number
    DWORD serialNumber;
    if (GetVolumeInformationA("C:\\", NULL, 0, &serialNumber, NULL, NULL, NULL, 0)) {
        sprintf_s(volumeSerial, "%08X", serialNumber);
    }

    // Computer Name
    DWORD computerNameSize = sizeof(computerName);
    GetComputerNameA(computerName, &computerNameSize);

    // MAC Address
    IP_ADAPTER_INFO adapterInfo[16];
    DWORD dwBufLen = sizeof(adapterInfo);

    if (GetAdaptersInfo(adapterInfo, &dwBufLen) == ERROR_SUCCESS) {
        PIP_ADAPTER_INFO pAdapterInfo = adapterInfo;
        while (pAdapterInfo) {
            if (pAdapterInfo->Type == MIB_IF_TYPE_ETHERNET ||
                pAdapterInfo->Type == IF_TYPE_IEEE80211) {
                sprintf_s(macAddress, "%02X%02X%02X%02X%02X%02X",
                    pAdapterInfo->Address[0], pAdapterInfo->Address[1],
                    pAdapterInfo->Address[2], pAdapterInfo->Address[3],
                    pAdapterInfo->Address[4], pAdapterInfo->Address[5]);
                break;
            }
            pAdapterInfo = pAdapterInfo->Next;
        }
    }

    // Birleştir ve hash oluştur
    std::string combined = std::string(volumeSerial) + "-" +
        std::string(computerName) + "-" +
        std::string(macAddress);

    unsigned long hash = 5381;
    for (char c : combined) {
        hash = ((hash << 5) + hash) + c;
    }

    sprintf_s(machineCode, "%08lX", hash);
    return std::string(machineCode);
}

// CryptoLens API ile aktivasyon kontrolü
bool CheckActivationWithCryptoLens(const std::wstring& activationCode, ActivationResult& result)
{
    result.success = false;

    if (!CheckInternetConnection()) {
        result.errorMessage = L"İnternet bağlantısı yok! Lütfen internet bağlantınızı kontrol edin.";
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"CryptoLens Client",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        NULL, NULL, 0);

    if (!hSession) {
        result.errorMessage = L"HTTP oturumu açılamadı!";
        return false;
    }

    WinHttpSetTimeouts(hSession, 5000, 5000, 5000, 5000);

    HINTERNET hConnect = WinHttpConnect(hSession, L"app.cryptolens.io",
        INTERNET_DEFAULT_HTTPS_PORT, 0);

    if (!hConnect) {
        result.errorMessage = L"CryptoLens sunucusuna bağlanılamadı!";
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::string machineCode = GetMachineCode();

    char key[256];
    size_t converted;
    wcstombs_s(&converted, key, sizeof(key), activationCode.c_str(), _TRUNCATE);

    std::string requestPath = "/api/key/activate?productId=" +
        std::to_string(CRYPTOLENS_PRODUCT_ID) +
        "&key=" + key +
        "&machineCode=" + machineCode +
        "&sign=true&signMethod=1&v=1";

    int pathLen = MultiByteToWideChar(CP_UTF8, 0, requestPath.c_str(), -1, NULL, 0);
    std::vector<wchar_t> wRequestPath(pathLen);
    MultiByteToWideChar(CP_UTF8, 0, requestPath.c_str(), -1, wRequestPath.data(), pathLen);

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wRequestPath.data(),
        NULL, NULL, NULL,
        WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH);

    if (!hRequest) {
        result.errorMessage = L"HTTP isteği oluşturulamadı!";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wstring authHeader = L"Authorization: Bearer ";
    authHeader += CRYPTOLENS_ACCESS_TOKEN;
    WinHttpAddRequestHeaders(hRequest, authHeader.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

    bool success = false;

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {

        if (WinHttpReceiveResponse(hRequest, NULL)) {
            DWORD dwSize = 0;
            std::string response;

            do {
                dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                if (dwSize == 0) break;

                std::vector<char> buffer(dwSize + 1);
                DWORD dwDownloaded = 0;

                if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                    response.append(buffer.data(), dwDownloaded);
                }
            } while (dwSize > 0);

            // JSON yanıtını parse et
            if (response.find("\"result\":0") != std::string::npos) {
                result.success = true;

                // Feature bits'i bul
                size_t featurePos = response.find("\"features\":");
                if (featurePos != std::string::npos) {
                    featurePos += 11;
                    int featureBits = atoi(response.substr(featurePos).c_str());
                    featureBits = featureBits & 0x3F; // Sadece ilk 6 bit
                    result.enabledFeatures = ParseFeatureBits(featureBits);
                }

                // Licensee
                size_t licenseePos = response.find("\"licensee\":\"");
                if (licenseePos != std::string::npos) {
                    licenseePos += 12;
                    size_t licenseeEnd = response.find("\"", licenseePos);
                    if (licenseeEnd != std::string::npos) {
                        std::string licensee = response.substr(licenseePos, licenseeEnd - licenseePos);
                        int wsize = MultiByteToWideChar(CP_UTF8, 0, licensee.c_str(), -1, NULL, 0);
                        std::vector<wchar_t> wbuf(wsize);
                        MultiByteToWideChar(CP_UTF8, 0, licensee.c_str(), -1, wbuf.data(), wsize);
                        result.licensee = wbuf.data();
                    }
                }

                // Expiry date
                size_t expiresPos = response.find("\"expires\":\"");
                if (expiresPos != std::string::npos) {
                    expiresPos += 11;
                    size_t expiresEnd = response.find("\"", expiresPos);
                    if (expiresEnd != std::string::npos) {
                        std::string expires = response.substr(expiresPos, expiresEnd - expiresPos);
                        int wsize = MultiByteToWideChar(CP_UTF8, 0, expires.c_str(), -1, NULL, 0);
                        std::vector<wchar_t> wbuf(wsize);
                        MultiByteToWideChar(CP_UTF8, 0, expires.c_str(), -1, wbuf.data(), wsize);
                        result.expiryDate = wbuf.data();
                    }
                }
            }
            else {
                size_t msgStart = response.find("\"message\":\"");
                if (msgStart != std::string::npos) {
                    msgStart += 11;
                    size_t msgEnd = response.find("\"", msgStart);
                    if (msgEnd != std::string::npos) {
                        std::string msg = response.substr(msgStart, msgEnd - msgStart);
                        int wsize = MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, NULL, 0);
                        std::vector<wchar_t> wbuf(wsize);
                        MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, wbuf.data(), wsize);
                        result.errorMessage = wbuf.data();
                    }
                }
                else {
                    result.errorMessage = L"Aktivasyon başarısız!";
                }
            }
        }
        else {
            result.errorMessage = L"Sunucudan yanıt alınamadı!";
        }
    }
    else {
        result.errorMessage = L"HTTP isteği gönderilemedi!";
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result.success;
}

// Feature bits çözümleme
std::vector<int> ParseFeatureBits(int featureBits)
{
    std::vector<int> enabledFeatures;
    int bitValue = 1;

    for (int i = 1; i <= 6; i++) {
        if (featureBits & bitValue) {
            enabledFeatures.push_back(i);
        }
        bitValue <<= 1;
    }
    return enabledFeatures;
}

// Feature ismi getir
const wchar_t* GetFeatureName(int featureId)
{
    switch (featureId) {
    case 1: return L"Temel Kullanım";
    case 2: return L"Deneme";
    case 3: return L"Panel Kullanım";
    case 4: return L"Gelişmiş Panel";
    case 5: return L"Pozitron";
    case 6: return L"AI Destekli Kullanım";
    default: return L"Bilinmeyen Özellik";
    }
}

// Feature aktif mi kontrolü
bool IsFeatureEnabled(int featureId, const std::vector<int>& enabledFeatures)
{
    for (int id : enabledFeatures) {
        if (id == featureId) return true;
    }
    return false;
}

// Paket seviyesini getir
std::wstring GetUserPackageLevel(const std::vector<int>& enabledFeatures)
{
    if (IsFeatureEnabled(6, enabledFeatures)) {
        return L"AI Destekli Premium Paket";
    }
    else if (IsFeatureEnabled(5, enabledFeatures)) {
        return L"Pozitron Profesyonel Paket";
    }
    else if (IsFeatureEnabled(4, enabledFeatures)) {
        return L"Gelişmiş Panel Paketi";
    }
    else if (IsFeatureEnabled(3, enabledFeatures)) {
        return L"Panel Kullanım Paketi";
    }
    else if (IsFeatureEnabled(2, enabledFeatures)) {
        return L"Deneme Paketi";
    }
    else if (IsFeatureEnabled(1, enabledFeatures)) {
        return L"Temel Kullanım Paketi";
    }
    return L"Aktivasyon Yok";
}

// Menüyü feature'lara göre güncelle
void UpdateMenuBasedOnFeatures()
{
    HMENU hMenu = GetMenu(g_hWnd);

    // Tüm menüleri varsayılan olarak pasif yap
    EnableMenuItem(hMenu, ID_PANEL_KULLANIM, MF_GRAYED);
    EnableMenuItem(hMenu, ID_GELISMIS_PANEL, MF_GRAYED);
    EnableMenuItem(hMenu, ID_POZITRON_OZELLIK, MF_GRAYED);
    EnableMenuItem(hMenu, ID_AI_DESTEK, MF_GRAYED);

    // Feature'lara göre aktif et
    for (int featureId : g_EnabledFeatures) {
        switch (featureId) {
        case 3: // Panel Kullanım
            EnableMenuItem(hMenu, ID_PANEL_KULLANIM, MF_ENABLED);
            break;
        case 4: // Gelişmiş Panel
            EnableMenuItem(hMenu, ID_GELISMIS_PANEL, MF_ENABLED);
            EnableMenuItem(hMenu, ID_PANEL_KULLANIM, MF_ENABLED);
            break;
        case 5: // Pozitron
            EnableMenuItem(hMenu, ID_POZITRON_OZELLIK, MF_ENABLED);
            EnableMenuItem(hMenu, ID_GELISMIS_PANEL, MF_ENABLED);
            EnableMenuItem(hMenu, ID_PANEL_KULLANIM, MF_ENABLED);
            break;
        case 6: // AI Destekli
            EnableMenuItem(hMenu, ID_AI_DESTEK, MF_ENABLED);
            EnableMenuItem(hMenu, ID_POZITRON_OZELLIK, MF_ENABLED);
            EnableMenuItem(hMenu, ID_GELISMIS_PANEL, MF_ENABLED);
            EnableMenuItem(hMenu, ID_PANEL_KULLANIM, MF_ENABLED);
            break;
        }
    }

    if (IsFeatureEnabled(2, g_EnabledFeatures)) {
        SetWindowText(g_hWnd, L"Pozitron Koruma - Deneme Sürümü");
    }
}

// Aktivasyon başarılı mesajı
void ShowActivationSuccessMessage(HWND hWnd, const ActivationResult& result)
{
    std::wstring featureMsg = L"✅ Aktivasyon başarılı!\n\n";
    featureMsg += L"👤 Müşteri: " + result.licensee + L"\n";
    featureMsg += L"📅 Bitiş: " + result.expiryDate + L"\n";
    featureMsg += L"📦 Paket: " + GetUserPackageLevel(result.enabledFeatures) + L"\n\n";
    featureMsg += L"🔓 Aktif Özellikler (" + std::to_wstring(result.enabledFeatures.size()) + L"/6):\n";

    for (int featureId : result.enabledFeatures) {
        featureMsg += L"  ✓ ";
        featureMsg += GetFeatureName(featureId);
        featureMsg += L"\n";
    }

    if (result.enabledFeatures.size() < 6) {
        featureMsg += L"\n❌ Aktif Olmayan Özellikler:\n";
        for (int i = 1; i <= 6; i++) {
            bool found = false;
            for (int id : result.enabledFeatures) {
                if (id == i) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                featureMsg += L"  ✗ ";
                featureMsg += GetFeatureName(i);
                featureMsg += L"\n";
            }
        }
    }

    MessageBox(hWnd, featureMsg.c_str(), L"Aktivasyon Başarılı", MB_OK | MB_ICONINFORMATION);
}

// Activation Dialog İşleyicisi
INT_PTR CALLBACK ActivationDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDC_JOIN_DISCORD)
        {
            ShellExecute(NULL, L"open", L"https://discord.com/invite/dynimiac", NULL, NULL, SW_SHOW);
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDC_ACTIVATE_BUTTON)
        {
            WCHAR activationCode[100];
            GetDlgItemText(hDlg, IDC_ACTIVATION_CODE, activationCode, 100);

            if (wcslen(activationCode) == 0)
            {
                MessageBox(hDlg, L"Lütfen aktivasyon kodunu girin!", L"Hata", MB_OK | MB_ICONERROR);
                return (INT_PTR)TRUE;
            }

            SetDlgItemText(hDlg, IDC_ACTIVATION_STATUS, L"Aktivasyon kontrol ediliyor...");
            EnableWindow(GetDlgItem(hDlg, IDC_ACTIVATE_BUTTON), FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_JOIN_DISCORD), FALSE);

            ActivationResult result;

            if (CheckActivationWithCryptoLens(activationCode, result))
            {
                g_bActivated = true;
                g_EnabledFeatures = result.enabledFeatures;

                ShowActivationSuccessMessage(hDlg, result);
                UpdateMenuBasedOnFeatures();

                EndDialog(hDlg, IDOK);
            }
            else
            {
                EnableWindow(GetDlgItem(hDlg, IDC_ACTIVATE_BUTTON), TRUE);
                EnableWindow(GetDlgItem(hDlg, IDC_JOIN_DISCORD), TRUE);
                SetDlgItemText(hDlg, IDC_ACTIVATION_STATUS, L"");

                std::wstring fullError = L"Aktivasyon başarısız!\n\n";
                fullError += result.errorMessage;
                MessageBox(hDlg, fullError.c_str(), L"Hata", MB_OK | MB_ICONERROR);
            }
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}