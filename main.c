#include <stdio.h>
#include <windows.h>
#include <Windowsx.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include "font.h"

#define WINSIZE 900
#define GRIDSIZE 100
#define GRIDOFFSET 50
#define HIGHGRID (WINSIZE - GRIDOFFSET * 2)
#define SPACING (HIGHGRID / GRIDSIZE)
#define SLEEPCHANGE 20
#define MENUBUTTONTOP 300
#define MENUBUTTONSPACING 100
#define MENUBUTTONOFFSET 350
#define MENUBUTTONWIDTH 200
#define MENUBUTTONHEIGHT 50
#define TOPCONTROLS 380
#define CONTROLSPACING 30
#define MAXFILES 10

int globalRunning, globalIterating, screen, globalMode, gridLines, sleepTime, highlight;
unsigned char fileList[MAXFILES][MAX_PATH], *str;


struct Cell
{
	int state;
	unsigned char color;
} grid[GRIDSIZE][GRIDSIZE], gridTemp[GRIDSIZE][GRIDSIZE];

struct offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
};
struct offscreen_buffer GlobalBackbuffer;

struct window_dimension
{
	int Width;
	int Height;
};

struct window_dimension GetWindowDimension(HWND Window)
{
	struct window_dimension Result;
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;
	return(Result);
}
static struct window_dimension Dimension;
static HDC DeviceContext;

static void ResizeDIBSection(struct offscreen_buffer *buffer, int Width, int Height)
{
	if (buffer->Memory)
	{
		VirtualFree(buffer->Memory, 0, MEM_RELEASE);
	}
	buffer->Width = Width;
	buffer->Height = Height;
	int BytesPerPixel = 4;
	buffer->Info.bmiHeader.biSize = sizeof(buffer->Info.bmiHeader);
	buffer->Info.bmiHeader.biWidth = buffer->Width;
	buffer->Info.bmiHeader.biHeight = -buffer->Height;
	buffer->Info.bmiHeader.biPlanes = 1;
	buffer->Info.bmiHeader.biBitCount = 32;
	buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = (buffer->Width*buffer->Height) * BytesPerPixel;
	buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	buffer->Pitch = Width*BytesPerPixel;
}

void Display()
{
	StretchDIBits(DeviceContext,
		0, 0, GlobalBackbuffer.Width, GlobalBackbuffer.Height,
		0, 0, GlobalBackbuffer.Width, GlobalBackbuffer.Height,
		GlobalBackbuffer.Memory,
		&GlobalBackbuffer.Info,
		DIB_RGB_COLORS, SRCCOPY);
};

void ClearScreen()
{
	unsigned char *Row = (unsigned char *)GlobalBackbuffer.Memory + GlobalBackbuffer.Pitch;
	for (int Y = 0; Y < 910; Y++)
	{
		unsigned int *Pixel = (unsigned int *)Row;
		for (int X = 0; X < 910; X++)
		{
			*Pixel++ = ((0 << 8) | 0 | (0 << 16));
		}
		Row += GlobalBackbuffer.Pitch;
	}
};

void ResetGrid()
{
	for (int X = 0; X < GRIDSIZE; X++)
	{
		for (int Y = 0; Y < GRIDSIZE; Y++)
		{
			grid[X][Y].state = gridTemp[X][Y].state = 0;
			grid[X][Y].color = gridTemp[Y][Y].color = 255;
		}
	}
};

void Randomize(int size)
{
	if (size < GRIDSIZE)
	{
		ResetGrid();
	}
	for (int X = (GRIDSIZE - size) / 2; X < (GRIDSIZE + size) / 2; X++)
	{
		for (int Y = (GRIDSIZE - size) / 2; Y < (GRIDSIZE + size) / 2; Y++)
		{
			int chance = 2;
			grid[X][Y].state = rand() % chance;
			if (grid[X][Y].state < chance - 1)
			{
				grid[X][Y].state = 0;
			}
			else
			{
				grid[X][Y].state = 1;
			}

			if (grid[X][Y].state == 1)
			{
				switch (globalMode)
				{
					case 0:
						grid[X][Y].color = 255;
						break;
					case 1:
						if (rand() % 2 == 0)
						{
							grid[X][Y].color = 255;
						}
						else
						{
							grid[X][Y].color = 0;
						}
						break;
					case 2:
						grid[X][Y].color = rand() % 256;
						break;
				}
			}
		}
	}
};

void RenderRectangle(int xOffset, int yOffset, int width, int height,
	int red, int green, int blue)
{
	unsigned char *Row = (unsigned char *)GlobalBackbuffer.Memory + GlobalBackbuffer.Pitch * yOffset;
	for (int Y = 0; Y < height; Y++)
	{
		unsigned int *Pixel = (unsigned int *)Row + xOffset;
		for (int X = 0; X < width; X++)
		{
			*Pixel++ = ((green << 8) | blue | (red << 16));
		}
		Row += GlobalBackbuffer.Pitch;
	}
};

void RenderString(unsigned char *str2, int size, int X, int Y, unsigned int red, unsigned int green, unsigned int blue)
{
	while (*str2 != '\0' && *str2 != 254)
	{
		for (int j = 0; j < 15; j++)
		{
			for (int k = 0; k < 7; k++)
			{
				if (characters[*str2][j][k] == 1)
				{
					RenderRectangle(X + k * size, Y + j * size, size, size, red, green, blue);
				}
			}
		}
		X += size * 9;
		str2++;
	}
};

void RenderMenuScreen()
{
	unsigned char color;
	for (int i = 0; i < 3; i++)
	{
		if (i == highlight)
		{
			color = 255;
		}
		else
		{
			color = 0;
		}
		RenderRectangle(MENUBUTTONOFFSET, MENUBUTTONTOP + MENUBUTTONSPACING * i, MENUBUTTONWIDTH, MENUBUTTONHEIGHT, color, color, color);
	}
	RenderString("Conway's game of life", 3, 170, 150, 255, 255, 255);
	RenderString("Use arrow keys to switch modes, enter to start", 1, 240, 260, 255, 255, 255);

	RenderString("Controls:", 1, 70, TOPCONTROLS - 30, 255, 255, 255);
	RenderString("Space: Start and stop", 1, 10, TOPCONTROLS, 255, 255, 255);
	RenderString("C: Clear grid", 1, 10, TOPCONTROLS + CONTROLSPACING, 255, 255, 255);
	RenderString("G: Toggle grid", 1, 10, TOPCONTROLS + CONTROLSPACING * 2, 255, 255, 255);
	RenderString("L: Load .cells preset", 1, 10, TOPCONTROLS + CONTROLSPACING * 3, 255, 255, 255);
	RenderString("R: Randomize", 1, 10, TOPCONTROLS + CONTROLSPACING * 4, 255, 255, 255);
	RenderString("Esc: Exit to menu screen", 1, 10, TOPCONTROLS + CONTROLSPACING * 5, 255, 255, 255);
	RenderString("Up/down arrow keys: Change speed", 1, 10, TOPCONTROLS + CONTROLSPACING * 6, 255, 255, 255);
	RenderString("Left mouse button: Toggle cell life", 1, 10, TOPCONTROLS + CONTROLSPACING * 7, 255, 255, 255);
	RenderString("Right mouse button: Toggle cell color", 1, 10, TOPCONTROLS + CONTROLSPACING * 8, 255, 255, 255);

	if (highlight == 0)
	{
		color = 0;
	}
	else
	{
		color = 255;
	}
	RenderString("Original", 2, MENUBUTTONOFFSET + 30, MENUBUTTONTOP + 10, color, color, color);

	if (highlight == 1)
	{
		color = 0;
	}
	else
	{
		color = 255;
	}
	RenderString("Immigration", 2, MENUBUTTONOFFSET + 2, MENUBUTTONTOP + MENUBUTTONSPACING + 10, color, color, color);

	if (highlight == 2)
	{
		color = 0;
	}
	else
	{
		color = 255;
	}
	RenderString("Rainbow", 2, MENUBUTTONOFFSET + 40, MENUBUTTONTOP + MENUBUTTONSPACING * 2 + 10, color, color, color);
};

void RenderLoadScreen()
{
	int yPos = 1;
	unsigned char color;
	for (int i = 0; i < MAXFILES; i++)
	{
		strcpy(str, fileList[i]);
		unsigned char *pathPointer = str;
		if (*pathPointer == NULL)
		{
			if (highlight > i - 1)
			{
				highlight = i - 1;
			}
			break;
		}
		pathPointer += strlen(fileList[i]) - 6;
		*pathPointer = 254;
		while (*(pathPointer - 1) != '\\')
		{
			pathPointer--;
		}
		if (highlight == i)
		{
			color = 255;
		}
		else
		{
			color = 0;
		}
		RenderRectangle(8, yPos, (strlen(pathPointer) - 6) * 10, 16, color, color, color);
		switch (color)
		{
			case 0:
				color = 255;
				break;
			case 255:
				color = 0;
				break;
		}
		RenderString(pathPointer, 1, 10, yPos, color, color, color);
		yPos += 20;
		if (highlight == i)
		{
			FILE *fp;
			fp = fopen(fileList[highlight], "r");
			unsigned char *strStart;
			unsigned char character;
			int yPos = 10;
			while ((character = fgetc(fp)) == '!')
			{
				strStart = str;
				while ((character = fgetc(fp)) != '\n')
				{
					*(strStart++) = character;
				}
				*strStart = 254;
				RenderString(str, 1, 200, yPos, 255, 255, 255);
				yPos += 20;
			}
			fclose(fp);
		}
	}
};

void Iterate()
{
	for (int Y = 0; Y < GRIDSIZE; Y++)
	{
		for (int X = 0; X < GRIDSIZE; X++)
		{
			int pop = 0;
			int colorTotal = 0;
			gridTemp[X][Y].color = grid[X][Y].color;
			for (int Y2 = Y - 1; Y2 <= Y + 1; Y2++)
			{
				for (int X2 = X - 1; X2 <= X + 1; X2++)
				{
					if ((X2 == X && Y2 == Y) || X2 < 0 || Y2 < 0 || X2 > GRIDSIZE - 1 || Y2 > GRIDSIZE - 1)
					{
						continue;
					}
					if (grid[X2][Y2].state == 1)
					{
						pop++;
						if (globalMode == 2)//rainbow
						{
							colorTotal += grid[X2][Y2].color;
						}
						else if (grid[X2][Y2].color == 0 && globalMode == 1)//immigration
						{
							colorTotal++;
						}
					}
				}
			}

			if (grid[X][Y].state == 1)
			{
				if (pop < 2 || pop > 3)
				{
					gridTemp[X][Y].state = 0;
				}
				else
				{
					gridTemp[X][Y].state = 1;
				}
			}
			if (grid[X][Y].state == 0)
			{
				if (pop == 3)
				{
					gridTemp[X][Y].state = 1;
					switch (globalMode)
					{
						case 0:
							gridTemp[X][Y].color = 255;
							break;
						case 1:
							if (colorTotal > 1)
							{
								gridTemp[X][Y].color = 0;
							}
							else
							{
								gridTemp[X][Y].color = 255;
							}
							break;
						case 2:
							gridTemp[X][Y].color = colorTotal / 3;
							break;
					}
				}
				else
				{
					gridTemp[X][Y].state = 0;
				}
			}
		}
	}
	for (int Y = 0; Y < GRIDSIZE; Y++)
	{
		for (int X = 0; X < GRIDSIZE; X++)
		{
			grid[X][Y] = gridTemp[X][Y];
		}
	}
};

void RenderGrid()
{
	unsigned char *Row = (unsigned char *)GlobalBackbuffer.Memory + GlobalBackbuffer.Pitch * GRIDOFFSET;
	for (int Y = 0; Y <= HIGHGRID; Y++)
	{
		unsigned int *Pixel = (unsigned int *)Row + GRIDOFFSET;
		for (int X = 0; X <= HIGHGRID; X++)
		{
			if (X % SPACING == 0 || Y % SPACING == 0)
			{
				if (gridLines == 1 || X == 0 || Y == 0 || X == HIGHGRID || Y == HIGHGRID)
				{
					*Pixel = ((75 << 16) | (75 << 8) | 75);
				}
				else
				{
					*Pixel = ((0 << 16) | (0 << 8) | 0);
				}

				if (X == HIGHGRID / 2 || Y == HIGHGRID / 2)
				{
					*Pixel = ((127 << 16) | (0 << 8) | 0);
				}
			}
			else
			{
				if (grid[X / SPACING][Y / SPACING].state == 0)
				{
					*Pixel = ((0 << 16) | (0 << 8) | 0);
				}
				else
				{
					*Pixel = ((grid[X / SPACING][Y / SPACING].color << 16) | (grid[X / SPACING][Y / SPACING].color << 8) | 255);
				}
			}
			*Pixel++;
		}
		Row += GlobalBackbuffer.Pitch;
	}
 };

LRESULT CALLBACK WindowCallback(
	HWND   Window,
	UINT   Message,
	WPARAM wParam,
	LPARAM lParam)
{
	LRESULT Result = 0;
	struct window_dimension Dimension = GetWindowDimension(Window);
	switch (Message)
	{
		case WM_LBUTTONDOWN:
		{
			if (globalIterating == 0 && screen == 0)
			{
				int xPos = GET_X_LPARAM(lParam);
				int yPos = GET_Y_LPARAM(lParam);
				xPos -= GRIDOFFSET;
				yPos -= GRIDOFFSET;
				switch (grid[xPos / SPACING][yPos / SPACING].state)
				{
					case 0:
						grid[xPos / SPACING][yPos / SPACING].state = 1;
						grid[xPos / SPACING][yPos / SPACING].color = 255;
						break;
					case 1:
						grid[xPos / SPACING][yPos / SPACING].state = 0;
						break;
				}
				RenderGrid();
			}
		}break;

		case WM_RBUTTONDOWN:
		{
			if (globalIterating == 0 && screen == 0 && globalMode != 0)
			{
				int xPos = GET_X_LPARAM(lParam) - GRIDOFFSET;
				int yPos = GET_Y_LPARAM(lParam) - GRIDOFFSET;
				if (grid[xPos / SPACING][yPos / SPACING].state == 1)
				{
					if (grid[xPos / SPACING][yPos / SPACING].color < 128)
					{
						grid[xPos / SPACING][yPos / SPACING].color = 255;
					}
					else
					{
						grid[xPos / SPACING][yPos / SPACING].color = 0;
					}
				}
				RenderGrid();
			}
		}break;

		case WM_KEYDOWN:
			switch (wParam)
			{
				case VK_SPACE:
					switch (globalIterating)
					{
						case 0:
							globalIterating = 1;
							break;
						case 1:
							globalIterating = 0;
							break;
					}
					break;

				case VK_UP:
					switch (screen)
					{
						case 0:
							sleepTime /= 2;
							break;
						case 1:
							if (highlight > 0)
							{
								highlight--;
							}
							RenderMenuScreen();
							break;
						case 2:
							if (highlight > 0)
							{
								highlight--;
							}
							ClearScreen();
							RenderLoadScreen();
							break;
					}
					break;

				case VK_DOWN:
					switch (screen)
					{
						case 0:
							if (sleepTime == 0)
							{
								sleepTime = 1;
							}
							else
							{
								sleepTime *= 2;
							}
							break;
						case 1:
							if (highlight < 2)
							{
								highlight++;
							}
							RenderMenuScreen();
							break;
						case 2:
							highlight++;
							ClearScreen();
							RenderLoadScreen();
							break;
					}
					break;

				case VK_ESCAPE:
					screen = 1;
					globalIterating = 0;
					ClearScreen();
					highlight = 0;
					RenderMenuScreen();
					break;

				case VK_RETURN:
					switch (screen)
					{
						case 1:
							screen = 0;
							globalIterating = 0;
							ClearScreen();
							ResetGrid();
							globalMode = highlight;
							RenderGrid();
							break;
						case 2:
							FILE *fp;
							fp = fopen(fileList[highlight], "r");
							int X, Y, length, skip;
							X = Y = length = skip = 0;
							ResetGrid();
							while (X < GRIDSIZE)
							{
								unsigned char state = fgetc(fp);
								if (skip == 1)
								{
									if (state == '\n')
									{
										skip = 0;
									}
									continue;
								}
								switch (state)
								{
									case 255:
										X = GRIDSIZE;
										break;
									case '.':
										gridTemp[X++][Y].state = 0;
										break;
									case 'O':
										gridTemp[X++][Y].state = 1;
										break;
									case '\n':
										if (X > length)
										{
											length = X;
										}
										X = 0;
										Y++;
										break;
									case '!':
										skip = 1;
										break;
								}
							}
							fclose(fp);
							int xShift = (GRIDSIZE - length) / 2;
							int yShift = (GRIDSIZE - Y) / 2;
							int height = Y;
							for (int Y = 0; Y <= height; Y++)
							{
								for (int X = 0; X <= length; X++)
								{
									grid[X + xShift][Y + yShift].state = gridTemp[X][Y].state;
									grid[X + xShift][Y + yShift].color = 255;
								}
							}
							globalIterating = 0;
							screen = 0;
							ClearScreen();
							RenderGrid();
							break;
					}
					break;

				case 0x43: //c, clears
					if (screen == 0)
					{
						ResetGrid();
						globalIterating = 0;
						RenderGrid();
					}
					break;

				case 0x47: //g, toggles grid
					switch (gridLines)
					{
						case 0:
							gridLines = 1;
							break;
						case 1:
							gridLines = 0;
							break;
					}
					RenderGrid();
					break;

				case 0x4C: //l, loads
					if (screen == 0)
					{
						unsigned char *pathPointer;
						pathPointer = str;
						GetModuleFileName(NULL, str, MAX_PATH);
						pathPointer += strlen(str);
						while (*(pathPointer - 1)  != '\\')
						{
							pathPointer--;
						}
						strcpy(pathPointer, "*.cells");
						WIN32_FIND_DATA data;
						HANDLE h = FindFirstFile(str, &data);
						if (GetLastError() == ERROR_FILE_NOT_FOUND)
						{
							FindClose(h);
							break;
						}
						strcpy(pathPointer, data.cFileName);
						strcpy(fileList[0], str);
						for (int i = 1; i < MAXFILES; i++)
						{
							FindNextFile(h, &data);
							if (GetLastError() == ERROR_NO_MORE_FILES)
							{
								FindClose(h);;
								break;
							}
							strcpy(pathPointer, data.cFileName);
							strcpy(fileList[i], str);
						}
						screen = 2;
						highlight = 0;
						ClearScreen();
						RenderLoadScreen();
					}
					break;

				case 0x52: //r, randomize
					if (screen == 0)
					{
						Randomize(100);
						globalIterating = 0;
						RenderGrid();
					}
					break;
			} break;

		case WM_DESTROY:
			globalRunning = 0;
			free(str);
			break;

		case WM_CLOSE:
			globalRunning = 0;
			free(str);
			break;

		case WM_ACTIVATEAPP:
			break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC devicecontext = BeginPaint(Window, &Paint);
			Display();
			EndPaint(Window, &Paint);
		} break;

		default:
			Result = DefWindowProc(Window, Message, wParam, lParam);
			break;

	return(Result);
	}
}

int CALLBACK WinMain(HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR     CmdLine,
	int       CmdShow)
{
	WNDCLASS wc = { 0 };
	ResizeDIBSection(&GlobalBackbuffer, WINSIZE + 17, WINSIZE + 41);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WindowCallback;
	wc.hInstance = Instance;
	wc.lpszClassName = "LifeWindowClass";
	RegisterClass(&wc);
	HWND Window =
		CreateWindowEx(
			0,
			wc.lpszClassName,
			"Game of Life",
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			WINSIZE + 17,
			WINSIZE + 41,
			0,
			0,
			Instance,
			0);
	DeviceContext = GetDC(Window);
	srand(time(NULL));
	globalRunning = 1;
	globalIterating = globalMode = gridLines = highlight = 0;
	screen = 1; //0 is main grid, 1 is menu, 2 is load list
	str = (unsigned char *)malloc(MAX_PATH);
	int MSElapsed = 0;
	sleepTime = 128; //milliseconds
	ResetGrid();
	RenderMenuScreen();

	LARGE_INTEGER perfCountFrequencyResult;
	QueryPerformanceFrequency(&perfCountFrequencyResult);
	long long perfCountFrequency = perfCountFrequencyResult.QuadPart;
	LARGE_INTEGER lastCounter;
	QueryPerformanceCounter(&lastCounter);
	while (globalRunning)
	{
		MSG Message;
		while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
		{
			if (Message.message == WM_QUIT)
			{
				globalRunning = 0;
			}
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}

		switch (screen)
		{
			case 0:
				if (globalIterating == 1)
				{
					Sleep(1);
					LARGE_INTEGER endCounter;
					QueryPerformanceCounter(&endCounter);
					long long counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
					MSElapsed += (1000 * counterElapsed) / perfCountFrequency;
					lastCounter = endCounter;
					if (MSElapsed >= sleepTime)
					{
						Iterate();
						RenderGrid();
						MSElapsed = 0;
					}
				}
				else
				{
					Sleep(50);
				}
				break;
			default:
				Sleep(50);
				break;
		}
		Display();
	}
	return 0;
}