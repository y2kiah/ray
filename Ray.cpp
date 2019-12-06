// RAY CASTER ENGINE by Jeff Kiah

#include <STDIO.H>
#include <STDLIB.H>
#include <STRING.H>
#include <CONIO.H>
#include <MATH.H>
#include <TIME.H>
//#include <ALLEGRO.H>
#include "DAT1.H"
#include "MAP.H"

#define RESX         320
#define RESY         200
#define CELLSIZE     64
#define CELLAND      63
#define CELLSHIFT    6
#define MINDIST      24
#define AREAX        25
#define AREAY        25
#define ANGLEINC     .25
#define DEGTORAD     .0174532925
#define CLOSED       0
#define CLOSING      1
#define OPEN         2
#define OPENING      3
#define LOCKED       4
#define CONTROLLED   5

#define ANGLE6       26
#define ANGLE30      120
#define ANGLE45      180
#define ANGLE90      360
#define ANGLE135     540
#define ANGLE180     720
#define ANGLE225     900
#define ANGLE270     1080
#define ANGLE315     1260
#define ANGLE360     1440

DATAFILE * Dat1;
BITMAP   * FloorTile[5];
BITMAP   * WallTile[15];

typedef struct {
	int X, Y, CellX, CellY, MoveX, MoveY, Angle, Health, VLook;
	unsigned char Type, Weapon, LightLevel, Height;
	unsigned char Weapons[10];
} Player_Type;

typedef struct {
	int X, Y, Angle, Health;
	unsigned char Type;
} Creature_Type;

typedef struct {
	int X, Y;
	char Type;
} Object_Type;

typedef struct {
	int Damage, Defense, Condition;
	char Type;
} Weapon_Type;

typedef struct {
	int CellX, CellY, Offset;
	unsigned char State, TexNumber, Delay;
} Door_Type;

typedef struct {
	int CellX, CellY;
	unsigned char TexColumn, TexNumber;
	double Dist;
} Wall_Collision_Type;

Player_Type Player;
Door_Type Door[20];
Wall_Collision_Type Collision[10];

double Sin[1440];
double Cos[1440];
double Tan[1440];
double InvSin[1440];
double InvCos[1440];
double InvTan[1440];
double DeltaX[1440];
double DeltaY[1440];
double FloorDist[240][100];
double DistFix[240];

int frames;
long timer;

unsigned char *Screen = (unsigned char *)0xA0000;
unsigned char *Buffer = (unsigned char *)malloc(64000);

/////////////////////////////////////////////////////////////////////////////

void PutPixel(int X, int Y, unsigned char COLOR)
{
	Buffer[(Y<<6)+(Y<<8)+X] = COLOR;
}

int FindDoor(int CELLX, int CELLY, int NUMDOORS)
{
///// Check if door is already in the list, return number in list
	for (int DCount = 0; DCount < NUMDOORS; DCount++) {
		if (Door[DCount].CellX == CELLX && Door[DCount].CellY == CELLY) return DCount;
	}

///// No Door Found
	return -1;
}

int CheckDoor(int NUMACTIVEDOORS)
{
///// If door is not in list, put it there
	int XOffset = 0;
	int YOffset = 0;
	if (Player.Angle <= ANGLE45 || Player.Angle > ANGLE315) YOffset = 1;
	else if (Player.Angle <= ANGLE135) XOffset = 1;
	else if (Player.Angle <= ANGLE225) YOffset = -1;
	else if (Player.Angle <= ANGLE315) XOffset = -1;
	int DoorCellX = (Player.X >> CELLSHIFT) + XOffset;
	int DoorCellY = (Player.Y >> CELLSHIFT) + YOffset;

	if (WallMap[DoorCellX][DoorCellY] > 11) {
		if (FindDoor(DoorCellX,DoorCellY,NUMACTIVEDOORS) == -1) {
			Door[NUMACTIVEDOORS].CellX = DoorCellX;
			Door[NUMACTIVEDOORS].CellY = DoorCellY;
			Door[NUMACTIVEDOORS].State = OPENING;
			Door[NUMACTIVEDOORS].Delay = 100;
			Door[NUMACTIVEDOORS].Offset = 0;
			NUMACTIVEDOORS++;
		}
	}

	return NUMACTIVEDOORS;
}

int UpdateDoors(int NUMACTIVEDOORS)
{
	int DCount = 0;
	while (DCount < NUMACTIVEDOORS) {
		switch (Door[DCount].State) {
		case OPENING :
			if (Door[DCount].Offset < 57) Door[DCount].Offset += 3;
			else Door[DCount].State = OPEN;
		break;
		case OPEN :
			if (Door[DCount].Delay > 0) Door[DCount].Delay--;
			else Door[DCount].State = CLOSING;
		break;
		case CLOSING :
			if (Door[DCount].Offset > 0) Door[DCount].Offset -= 3;
			else Door[DCount].State = CLOSED;
		break;
		case CLOSED :
			NUMACTIVEDOORS--;
			for (int DSwitch = DCount; DSwitch < NUMACTIVEDOORS; DSwitch++)
				Door[DSwitch] = Door[DSwitch + 1];
		break;
		}

		if (Door[DCount].State == OPEN) {
			Door[DCount].TexNumber = WallMap[Door[DCount].CellX][Door[DCount].CellY];
			WallMap[Door[DCount].CellX][Door[DCount].CellY] = 0;
		}

		DCount++;
	}
	
	return NUMACTIVEDOORS;
}

void PlayGame(void)
{
///// Wall Variables
	double XInter,YInter,XDelta,YDelta,Dist,Dist1,Dist2,Step,TilePixel;
	int TexCol,TexCol1,TexCol2,CellX,CellY,CellX1,CellX2,CellY1,CellY2,YStart,Stretch,WallSize;
	char NextX,NextY;

///// Floor And Ceiling Variables
	int WorldX,WorldY,TileX,TileY;

///// Misc. Variables
	int SubX,SubY,Col,ColAdd,TopCol,LightLevel,NumCol,NumActiveDoors = 0;
	int *MouseMoveX,*MouseMoveY;
	bool FloorDone;

	for (int Count = 0; Count < 64000; Count++) Buffer[Count] = 0;
	timer = time(0);

	while (!key[KEY_ESC]) {
////// Cast Rays
		int FirstYInt1 = (Player.Y >> CELLSHIFT) << CELLSHIFT;
		int FirstYInt2 = ((Player.Y >> CELLSHIFT) << CELLSHIFT) + CELLSIZE;
		int FirstXInt1 = ((Player.X >> CELLSHIFT) << CELLSHIFT) + CELLSIZE;
		int FirstXInt2 = (Player.X >> CELLSHIFT) << CELLSHIFT;

		int RayAngle = Player.Angle - ANGLE30;
		if (RayAngle < 0) RayAngle += ANGLE360;

		for (int Ray = 0; Ray < 240; Ray++) {
			if (RayAngle % ANGLE90 == 0) RayAngle++;
			if (RayAngle >= ANGLE360) RayAngle -= ANGLE360;

			NumCol = 0;
			for (;;) {
				if (RayAngle > ANGLE270 || RayAngle < ANGLE90) {
					XInter = Player.X + Tan[RayAngle] * (FirstYInt2 - Player.Y);
					YInter = FirstYInt2;
					YDelta = CELLSIZE;
					XDelta = DeltaX[RayAngle];
					NextY = 0;
				} else {
					XInter = Player.X + Tan[RayAngle] * (FirstYInt1 - Player.Y);
					YInter = FirstYInt1;
					YDelta = -CELLSIZE;
					XDelta = -DeltaX[RayAngle];
					NextY = -1;
				}

				for (;;) {
					CellX1 = (int)XInter >> CELLSHIFT;
					CellY1 = ((int)YInter >> CELLSHIFT) + NextY;
					if (WallMap[CellX1][CellY1] != 0) {
						if (WallMap[CellX1][CellY1] > 9) {
							Dist1 = (XInter + (XDelta * 0.5) - Player.X) * InvSin[RayAngle];
							TexCol1 = (int)(XInter + (XDelta * 0.5)) & CELLAND;
						} else {
							Dist1 = (XInter - Player.X) * InvSin[RayAngle];
							TexCol1 = (int)XInter & CELLAND;
						}

						break;
					}
					
					XInter += XDelta;
					YInter += YDelta;

				}

				if (RayAngle < ANGLE180) {
					YInter = Player.Y + (FirstXInt1 - Player.X) * InvTan[RayAngle];
					XInter = FirstXInt1;
					XDelta = CELLSIZE;
					YDelta = DeltaY[RayAngle];
					NextX = 0;
				} else {
					YInter = Player.Y + (FirstXInt2 - Player.X) * InvTan[RayAngle];
					XInter = FirstXInt2;
					XDelta = -CELLSIZE;
					YDelta = -DeltaY[RayAngle];
					NextX = -1;
				}

				for (;;) {
					CellX2 = ((int)XInter >> CELLSHIFT) + NextX;
					CellY2 = (int)YInter >> CELLSHIFT;
					if (WallMap[CellX2][CellY2] != 0) {
						if (WallMap[CellX2][CellY2] > 9) {
							Dist2 = (YInter + (YDelta * 0.5) - Player.Y) * InvCos[RayAngle];
							TexCol2 = (int)(YInter + (YDelta * 0.5)) & CELLAND;
						} else {
							Dist2 = (YInter - Player.Y) * InvCos[RayAngle];
							TexCol2 = (int)YInter & CELLAND;
						}

						break;
					}

					XInter += XDelta;
					YInter += YDelta;
				}

				if (Dist1 < Dist2) {
					Collision[NumCol].Dist = Dist1;
					Collision[NumCol].TexColumn = TexCol1;
					Collision[NumCol].CellX = CellX1;
					Collision[NumCol].CellY = CellY1;
				} else {
					Collision[NumCol].Dist = Dist2;
					Collision[NumCol].TexColumn = TexCol2;
					Collision[NumCol].CellX = CellX2;
					Collision[NumCol].CellY = CellY2;
				}

///// Check to see if the block is transparent or solid
				Collision[NumCol].TexNumber = WallMap[Collision[NumCol].CellX][Collision[NumCol].CellY];

				if (WallMap[Collision[NumCol].CellX][Collision[NumCol].CellY] < 10) break;

///// If it is a non-transparent door, see if it is closed
				if (WallMap[Collision[NumCol].CellX][Collision[NumCol].CellY] > 12)
					if (FindDoor(Collision[NumCol].CellX,Collision[NumCol].CellY,NumActiveDoors) == -1) break;

///// Set the map value to 0 so we can see through it
				WallMap[Collision[NumCol].CellX][Collision[NumCol].CellY] = 0;
				NumCol++;
			}

///// Draw all wall collisions to the screen
			FloorDone = false;
			for (int ColCount = NumCol; ColCount > -1; ColCount--) {
				Dist = Collision[ColCount].Dist * DistFix[Ray];
				TexCol = Collision[ColCount].TexColumn;
				CellX = Collision[ColCount].CellX;
				CellY = Collision[ColCount].CellY;
				WallMap[CellX][CellY] = Collision[ColCount].TexNumber;

				WallSize = (int)(13440 / Dist);
				WallSize -= (WallSize & 1);

///// Draw Wall
				Step = CELLSIZE / (double)WallSize;

				if (WallSize <= 200) {
					TilePixel = 0;
					Stretch = WallSize;
					YStart = 100 - (Stretch >> 1);
				} else {
					TilePixel = Step * ((WallSize - 200) >> 1);
					Stretch = 200;
					YStart = 0;
				}

///// If it is a door, see if it has to be displaced vertically
				if (WallMap[CellX][CellY] > 11) {
					int DoorNum = FindDoor(CellX,CellY,NumActiveDoors);
					if (DoorNum != -1) {
						TilePixel += Door[DoorNum].Offset;
						Stretch -= ((Door[DoorNum].Offset * WallSize) >> 6) - ((WallSize - Stretch) >> 1);
					}
				}

				LightLevel = (int)Dist >> Player.LightLevel;
				for (int Count = YStart; Count < YStart+Stretch; Count++) {
					Col = WallTile[WallMap[CellX][CellY]-1]->line[(int)TilePixel][TexCol];

					if (Col != 0) {
						TopCol = Col + 15 - (Col & 15);

						if (LightMap[CellX][CellY] > 0 && LightMap[CellX][CellY] - 1 < LightLevel)
							Col += LightMap[CellX][CellY] - 1;
						else Col += LightLevel;
						if (Col > TopCol) Col = TopCol;

						PutPixel(Ray,Count,Col);
					}

					TilePixel += Step;
				}

///// Draw Floor And Ceiling
				if (FloorDone == false) {
					FloorDone = true;
					for (int Row = WallSize >> 1; Row < 100; Row++) {
						Dist = FloorDist[Ray][Row];

						WorldX = (int)(Player.X + Sin[RayAngle] * Dist);
						WorldY = (int)(Player.Y + Cos[RayAngle] * Dist);
						TileX = WorldX & CELLAND;
						TileY = WorldY & CELLAND;
						CellX = WorldX >> CELLSHIFT;
						CellY = WorldY >> CELLSHIFT;
						LightLevel = (int)Dist >> Player.LightLevel;

						if (LightMap[CellX][CellY] > 0 && LightMap[CellX][CellY] - 1 < LightLevel)
							ColAdd = LightMap[CellX][CellY] - 1;
						else ColAdd = LightLevel;

						Col = FloorTile[0]->line[TileY][TileX];
						TopCol = Col + 15 - (Col & 15);
						Col += ColAdd;
						if (Col > TopCol) Col = TopCol;

						PutPixel(Ray,99-Row,Col);

						Col = FloorTile[FloorMap[CellX][CellY]]->line[TileY][TileX];
						TopCol = Col + 15 - (Col & 15);
						Col += ColAdd;
						if (Col > TopCol) Col = TopCol;

						PutPixel(Ray,100+Row,Col);
					}
				}
			}

			RayAngle++;
		}

///// Draw Sprites
/*    int dx = Creature.X - Player.X;
    int dy = Creature.Y - Player.Y;
    int a = (int)(asin(dx/dy) * (180/PI) / ANGLEINC);
    double d = abs(Tan(a));
    int s = (int)(35456 / d);
    int pa = Player.Angle + a;
    if (abs(pa) <= 30) {
      unsigned char *sprite_ptr = Sprite->line[0];
      for (int pixel

    }*/

///// Draw Screen
		dosmemput(Buffer,64000,(unsigned long)Screen);

///// Keyboard Inputs
		Player.MoveX = 0;
		Player.MoveY = 0;

		get_mouse_mickeys(MouseMoveX,MouseMoveY);

		if (key[KEY_UP]) {
			Player.MoveX += (int)(Sin[Player.Angle] * 15);
			Player.MoveY += (int)(Cos[Player.Angle] * 15);
		}
		if (key[KEY_DOWN]) {
			Player.MoveX -= (int)(Sin[Player.Angle] * 15);
			Player.MoveY -= (int)(Cos[Player.Angle] * 15);
		}
		if (key[KEY_LEFT]) {
			Player.MoveX -= (int)(Sin[(Player.Angle + ANGLE90) % ANGLE360] * 10);
			Player.MoveY -= (int)(Cos[(Player.Angle + ANGLE90) % ANGLE360] * 10);
		}
		if (key[KEY_RIGHT]) {
			Player.MoveX += (int)(Sin[(Player.Angle + ANGLE90) % ANGLE360] * 10);
			Player.MoveY += (int)(Cos[(Player.Angle + ANGLE90) % ANGLE360] * 10);
		}

//    if (key[KEY_W]) {
//      Player.Weapon++;
//      while (Player.Weapons[Player.Weapon] != 1) {
//        Player.Weapon++;
//        if (Player.Weapon == 9) Player.Weapon = 0;
//      }
//    }

		if (key[KEY_L]) Player.LightLevel++;

		if (mouse_b == 2 || key[KEY_SPACE]) NumActiveDoors = CheckDoor(NumActiveDoors);

///// Limitations
		NumActiveDoors = UpdateDoors(NumActiveDoors);

		Player.Angle += (int)(*MouseMoveX >> 1);

		if (Player.Angle >= ANGLE360) Player.Angle -= ANGLE360;
		else if (Player.Angle < 0) Player.Angle += ANGLE360;
		if (Player.Angle == 0) Player.Angle = 1;

		if (Player.LightLevel > 7) Player.LightLevel = 5;

		Player.X += Player.MoveX;
		Player.Y += Player.MoveY;

		Player.CellX = Player.X >> CELLSHIFT;
		Player.CellY = Player.Y >> CELLSHIFT;

		SubX = Player.X & CELLAND;
		SubY = Player.Y & CELLAND;

		if (Player.MoveX > 0) {
			if (WallMap[Player.CellX+1][Player.CellY] != 0 && SubX > (CELLSIZE - MINDIST))
			Player.X -= (SubX - (CELLSIZE - MINDIST));
		} else if (Player.MoveX < 0) {
	 		if (WallMap[Player.CellX-1][Player.CellY] != 0 && SubX < MINDIST)
			Player.X += (MINDIST - SubX);
		}
		if (Player.MoveY > 0) {
			if (WallMap[Player.CellX][Player.CellY+1] != 0 && SubY > (CELLSIZE - MINDIST))
			Player.Y -= (SubY - (CELLSIZE - MINDIST));
		} else if (Player.MoveY < 0) {
			if (WallMap[Player.CellX][Player.CellY-1] != 0 && SubY < MINDIST)
			Player.Y += (MINDIST - SubY);
		}

		for (int DCount = 0; DCount < NumActiveDoors; DCount++) {
			if (Door[DCount].State == OPEN)
				WallMap[Door[DCount].CellX][Door[DCount].CellY] = Door[DCount].TexNumber;
		}

		frames++;
	}
}

void InitGame(void)
{
///// Build Lookup Tables
	for (int Angle = 0; Angle < ANGLE360; Angle++) {
		Sin[Angle] = sin(Angle * ANGLEINC * DEGTORAD);
		Cos[Angle] = cos(Angle * ANGLEINC * DEGTORAD);
		Tan[Angle] = tan(Angle * ANGLEINC * DEGTORAD);
		InvSin[Angle] = 1 / sin(Angle * ANGLEINC * DEGTORAD);
		InvCos[Angle] = 1 / cos(Angle * ANGLEINC * DEGTORAD);
		InvTan[Angle] = 1 / tan(Angle * ANGLEINC * DEGTORAD);
		DeltaX[Angle] = CELLSIZE * Tan[Angle];
		DeltaY[Angle] = CELLSIZE * InvTan[Angle];
	}

	int Angle = ANGLE30;
	for (int Ray = 0; Ray < 120; Ray++) {
		for (int Row = 0; Row < 100; Row++) {
			FloorDist[Ray][Row] = (210 * (CELLSIZE/2) / (Row+1)) * InvCos[Angle];
			FloorDist[239-Ray][Row] = (210 * (CELLSIZE/2) / (Row+1)) * InvCos[Angle];
			DistFix[Ray] = Cos[Angle];
			DistFix[239-Ray] = Cos[Angle];
		}

		Angle--;
	}

///// Init Game Variables
	Player.Angle = ANGLE315;
	Player.X = (CELLSIZE * 6) - (CELLSIZE / 2);
	Player.Y = (CELLSIZE * 4) - (CELLSIZE / 2);
	Player.LightLevel = 5;
	Player.Weapon = 0;
	for (int w = 0; w < 4; w++) Player.Weapons[w] = 1;
	for (int w = 4; w < 10; w++) Player.Weapons[w] = 0;
}

void Init(void)
{
	allegro_init();
	Dat1 = load_datafile("DAT1.DAT");
	for (int Pic = 1; Pic < 6; Pic++) FloorTile[Pic-1] = (BITMAP*)Dat1[Pic].dat;
	for (int Pic = 6; Pic < 21; Pic++) WallTile[Pic-6] = (BITMAP*)Dat1[Pic].dat;
	install_keyboard();
	install_mouse();

/*  printf("---=== RAY CASTER ===---\n");
	printf("      By Jeff Kiah\n\n");
	printf("MOUSE ............ TURN LEFT/RIGHT\n");
	printf("UP, DOWN ......... WALK\n");
	printf("LEFT, RIGHT ...... STRAFE\n");
	printf("SPACE, MOUSE 2 ... OPEN\n");
	printf("L ................ CHANGE LIGHTING\n");
	printf("ESC .............. EXIT\n\n");
	printf("Press Enter to Continue\n");
	while (!key[KEY_ENTER]) {}*/

	set_gfx_mode(GFX_VGA,320,200,0,0);
	set_palette((RGB*)Dat1[DAT1__PAL].dat);
	clear(screen);
}

void DeInit(void)
{
	free(Buffer);
	unload_datafile(Dat1);
	allegro_exit();
	printf("%d FPS\n", frames / (time(0) - timer));
}

int main(void)
{
	Init();
	InitGame();
	PlayGame();
	DeInit();
	return 0;
}


