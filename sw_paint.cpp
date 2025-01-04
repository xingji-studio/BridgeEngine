#include "BrigeEngine.h"
#include "compile_set.h"

#if PLATFORM == PLATFORM_WINDOWS
#endif

/* ���� */
#if PLATFORM == PLATFORM_WINDOWS
// Windows
void bapi_line(int x0, int y0, int x1, int y1, char r, char g, char b)
{
	HDC hdc = GetDC(XBE_hWnd); // ��ȡ���ڵ�DC
	HPEN hPen = CreatePen(PS_SOLID, 1, RGB(r, g, b)); // ����ʵ�߻���
	SetBkMode(hdc, TRANSPARENT);
	HGDIOBJ hOld = SelectObject(hdc, hPen); // ������ѡ��DC��

	// ��ͼ����
	MoveToEx(hdc, x0, y0, NULL); // �ƶ�����ʼ��
	LineTo(hdc, x1, y1); // ���ߵ�(100, 100)

	// �ָ�ԭ���Ķ���
	SelectObject(hdc, hOld);
	DeleteObject(hPen); // ɾ�������Ķ��󣬱����ڴ�й©

	ReleaseDC(XBE_hWnd, hdc); // �ͷ�DC
#elif PLATFORM == PLATFORM_XJ380
// XJ380
void bapi_line(int x0, int y0, int x1, int y1, char r, char g, char b)
{
	// pass
#endif
	return;
}

/* ����Բ */
#if PLATFORM == PLATFORM_WINDOWS
// Windows
void bapi_ellipse(int x0, int y0, int x1, int y1, char r, char g, char b)
{
	HDC hdc = GetDC(XBE_hWnd); // ��ȡ���ڵ�DC
	HPEN hPen = CreatePen(PS_SOLID, 1, RGB(r, g, b)); // ����ʵ�߻���
	SetBkMode(hdc, TRANSPARENT);
	HGDIOBJ hOld = SelectObject(hdc, hPen); // ������ѡ��DC��

	// ��ͼ����
	Ellipse(hdc, x0, y0, x1, y1);

	// �ָ�ԭ���Ķ���
	SelectObject(hdc, hOld);
	DeleteObject(hPen); // ɾ�������Ķ��󣬱����ڴ�й©

	ReleaseDC(XBE_hWnd, hdc); // �ͷ�DC
#elif PLATFORM == PLATFORM_XJ380
// XJ380
void bapi_ellipse(int x0, int y0, int x1, int y1, char r, char g, char b)
{
	// pass
#endif
	return;
}

/* д�ֶ� */
#if PLATFORM == PLATFORM_WINDOWS
// Windows
void bapi_font(int x0, int y0, int size, LPCWSTR str, char r, char g, char b, LPCWSTR font)
{
	HDC hdc = GetDC(XBE_hWnd); // ��ȡ���ڵ�DC
	SetTextColor(hdc, RGB(r, g, b)); // ����ʵ�߻���
	SetBkMode(hdc, TRANSPARENT);
	HFONT hFont = CreateFont(size, 0, 
								0, 0, 
								FW_NORMAL, 
								0, 0, 0, 
								GB2312_CHARSET, 
								OUT_DEFAULT_PRECIS,
								CLIP_DEFAULT_PRECIS,
								DEFAULT_QUALITY,
								DEFAULT_PITCH | FF_DONTCARE,
								font);
	HGDIOBJ hOld = SelectObject(hdc, hFont); // ������ѡ��DC��

	// ��ͼ����
	TextOut(hdc, x0, y0, str, lstrlen(str));

	// �ָ�ԭ���Ķ���
	SelectObject(hdc, hOld);
	DeleteObject(hFont); // ɾ�������Ķ��󣬱����ڴ�й©

	ReleaseDC(XBE_hWnd, hdc); // �ͷ�DC
#elif PLATFORM == PLATFORM_XJ380
// XJ380
void bapi_font(int x0, int y0, LPCWSTR str, char r, char g, char b)
{
	// pass
#endif
	return;
}

/* ͼƬ */
#if PLATFORM == PLATFORM_WINDOWS
// Windows
void bapi_image(int x0, int y0, int xsize, int ysize, LPCWSTR file_path)
{
	HDC hdc = GetDC(XBE_hWnd);

	// ����ͼƬ
	HBITMAP hBitmap = (HBITMAP)LoadImage(NULL, 
										file_path, 
										IMAGE_BITMAP, 
										0, 0, 
										LR_LOADFROMFILE);

	// ����ͼƬ
	HDC memDC = CreateCompatibleDC(hdc);
	HGDIOBJ hOld = SelectObject(memDC, hBitmap);
	BitBlt(hdc, x0, y0, xsize, ysize, memDC, x0, y0, SRCCOPY);

	SelectObject(hdc, hOld);
	// �ͷ���Դ
	DeleteDC(memDC);
	DeleteObject(hBitmap);
	ReleaseDC(XBE_hWnd, hdc);
#elif PLATFORM == PLATFORM_XJ380
// XJ380
void bapi_image(int x0, int y0, int xsize, int ysize, LPCWSTR file_path)
{
	// pass
#endif
	return;
}