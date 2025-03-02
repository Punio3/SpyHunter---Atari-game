#define _USE_MATH_DEFINES
#include<math.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<time.h>

extern "C" {
#include"./SDL2-2.0.10/include/SDL.h"
#include"./SDL2-2.0.10/include/SDL_main.h"
}

#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480

typedef struct car {
	int type; //0-friend 1-enemy 2-main car
	double y;
	double x;
	double drawY; 
	int w;
	int h;
	int speedX;
	int speedY;
	int isOutRoad;
	int pushed;
	SDL_Surface* sprite;
}Car;

//do sprawdzania kolizji
typedef struct colision {
	Car* car;
	int type;
	int index;
}Colision;

typedef struct missle {
	double x;
	double y;
	double drawY;
	double distance;  
	double range; 
	int w;
	int h;
	int speedY;
	Uint32 outlineColor;
	Uint32 fillColor;
}Missle;

// narysowanie napisu txt na powierzchni screen, zaczynaj c od punktu (x, y)
// charset to bitmapa 128x128 zawieraj ca znaki
// draw a text txt on surface screen, starting from the point (x, y)
// charset is a 128x128 bitmap containing character images
void DrawString(SDL_Surface* screen, int x, int y, const char* text,
	SDL_Surface* charset) {
	int px, py, c;
	SDL_Rect s, d;
	s.w = 8;
	s.h = 8;
	d.w = 8;
	d.h = 8;
	while (*text) {
		c = *text & 255;
		px = (c % 16) * 8;
		py = (c / 16) * 8;
		s.x = px;
		s.y = py;
		d.x = x;
		d.y = y;
		SDL_BlitSurface(charset, &s, screen, &d);
		x += 8;
		text++;
	};
};


// narysowanie na ekranie screen powierzchni sprite w punkcie (x, y)
// (x, y) to punkt  rodka obrazka sprite na ekranie
// draw a surface sprite on a surface screen in point (x, y)
// (x, y) is the center of sprite on screen
void DrawSurface(SDL_Surface* screen, SDL_Surface* sprite, int x, int y) {
	SDL_Rect dest;
	dest.x = x;
	dest.y = y;
	dest.w = sprite->w;
	dest.h = sprite->h;
	SDL_BlitSurface(sprite, NULL, screen, &dest);
};


// rysowanie pojedynczego pixela
// draw a single pixel
void DrawPixel(SDL_Surface* surface, int x, int y, Uint32 color) {
	int bpp = surface->format->BytesPerPixel;
	Uint8* p = (Uint8*)surface->pixels + y * surface->pitch + x * bpp;
	*(Uint32*)p = color;
};


// rysowanie linii o d ugo ci l w pionie (gdy dx = 0, dy = 1) 
// b d  poziomie (gdy dx = 1, dy = 0)
// draw a vertical (when dx = 0, dy = 1) or horizontal (when dx = 1, dy = 0) line
void DrawLine(SDL_Surface* screen, int x, int y, int l, int dx, int dy, Uint32 color) {
	for (int i = 0; i < l; i++) {
		DrawPixel(screen, x, y, color);
		x += dx;
		y += dy;
	};
};


// rysowanie prostok ta o d ugo ci bok w l i k
// draw a rectangle of size l by k
void DrawRectangle(SDL_Surface* screen, int x, int y, int l, int k,
	Uint32 outlineColor, Uint32 fillColor) {
	int i;
	DrawLine(screen, x, y, k, 0, 1, outlineColor);
	DrawLine(screen, x + l - 1, y, k, 0, 1, outlineColor);
	DrawLine(screen, x, y, l, 1, 0, outlineColor);
	DrawLine(screen, x, y + k - 1, l, 1, 0, outlineColor);
	for (i = y + 1; i < y + k - 1; i++)
		DrawLine(screen, x + 1, i, l - 2, 1, 0, fillColor);
};
//sprawdzanie czy wartosc jest w przedziale
int valueInRange(int value, int min, int max) {
	if ((value >= min) && (value <= max))
		return 1;
	else
		return 0;
}
//sprawdz czy prostokaty sie na siebie nakladaja
int rectOverlap(SDL_Rect A, SDL_Rect B) {
	
	int xOverlap = valueInRange(A.x, B.x, B.x + B.w) ||
		valueInRange(B.x, A.x, A.x + A.w);

	int yOverlap = valueInRange(A.y, B.y, B.y + B.h) ||
		valueInRange(B.y, A.y, A.y + A.h);

	if (xOverlap == 1 && yOverlap == 1) {
		// typ kolizji
		if (valueInRange(A.y + A.h, B.y + 5, B.y + B.h))
			return 2; //boczna kolizja
		else if (valueInRange(A.y, B.y, B.y + B.h - 5))
			return 2; //boczna kolizja
		else
			return 1; //kazda inna kolizja
	}
	else
		return 0;
}
// sprawdzanie prostokatow asfaltu czy znajduja sie na ekranie
int CheckRec(SDL_Rect rec, int lowY, int highY) {
	if (highY >= rec.y && (rec.y - rec.h) >= lowY) {
		return 1; 
	}
	else if (lowY <= rec.y && rec.y <= highY) {
		return 2;
	}
	else if (lowY <= (rec.y - rec.h) && (rec.y - rec.h) < highY) {
		return 3;
	}
	else if (lowY >= (rec.y - rec.h) && highY <= rec.y) {
		return 4;
	}
	else {
		return 0;
	}
}
// szerokosc gornego prostokata (do spawnowania samochodu)
SDL_Rect FindTopRec(SDL_Rect drawnRoad[], int len) {
	int i;
	int max = 0;
	SDL_Rect rec;
	max = drawnRoad[0].w;

	for (i = 0; i < len; i++) {
		if (drawnRoad[i].w > max)
			max = drawnRoad[i].w;
		rec = drawnRoad[i];
	}

	return rec;
}
//rysowanie drogi
int DrawRoad(SDL_Surface* screen, const SDL_Rect road[], int len, Car* car, double playerY, SDL_Rect drawnRoad[],
	Uint32 outlineColor, Uint32 fillColor) {
	int i, rv;
	int x, y, w, h;
	int j = 0;
	car->isOutRoad = 0;
	memset(drawnRoad, 0, 10 * sizeof(SDL_Rect));

	for (i = 0; i < len; i++) {
		rv = CheckRec(road[i], (int)playerY, (int)(playerY + 480.0));
		if (rv == 1) {
			x = road[i].x;
			y = 480 - (road[i].y - playerY);
			w = road[i].w;
			h = road[i].h;
			DrawRectangle(screen, x, y, w, h, outlineColor, fillColor);
		}
		else if (rv == 2) {
			x = road[i].x;
			y = 480 - (road[i].y - playerY);
			w = road[i].w;
			h = road[i].y - playerY;
			DrawRectangle(screen, x, y, w, h, outlineColor, fillColor);
		}
		else if (rv == 3) {
			x = road[i].x;
			y = 0;
			w = road[i].w;
			h = road[i].h - (road[i].y - (playerY + 480));
			DrawRectangle(screen, x, y, w, h, outlineColor, fillColor);
		}
		else if (rv == 4) {
			x = road[i].x;
			y = 0;
			w = road[i].w;
			h = 480;
			DrawRectangle(screen, x, y, w, h, outlineColor, fillColor);
		}
		if (rv != 0) {
			drawnRoad[j].x = x;
			drawnRoad[j].y = y;
			drawnRoad[j].w = w;
			drawnRoad[j].h = h;
			j++;
		}

		if (rv != 0 && ((int)car->x < x || ((int)car->x + car->w) >(x + w))
			&& !((y + h > car->drawY + car->h && y > car->drawY + car->h) || (y + h < car->drawY && y < car->drawY))) {
			car->isOutRoad = 1;
		}
	}
	return j;
}
//losowanie wartosci
int random(int min, int max)
{
	int tmp;
	if (max >= min)
		max -= min;
	else
	{
		tmp = min - max;
		min = max;
		max = tmp;
	}
	return max ? (rand() % max + min) : min;
}
//liczba samochodow w tablicy
int CountCars(Car** carTab) {
	int i;
	int index = 0;
	for (i = 0; i < 100; i++) {
		if (carTab[i] != NULL)
			index = i;
	}
	return ++index;
}
//zwraca liczbe pociskow
int CountMissles(Missle** missleTab) {
	int i;
	int index = 0;
	for (i = 0; i < 20; i++) {
		if (missleTab[i] != NULL)
			index = i;
	}
	return ++index;
}
//spawnuje nowy samochod
void SpawnCar(Car** carTab, Car* car, double playerY, SDL_Surface* spriteF, SDL_Surface* spriteE, SDL_Rect topRec) {
	int i = 0;
	int zarodek;
	time_t tt;
	zarodek = time(&tt);
	srand(zarodek);
	Car* newCar = (Car*)malloc(sizeof(Car));
	while (carTab[i] != NULL)
		i++;
	newCar->type = rand() % 2;
	newCar->y = car->y + 480.0 + 54.0;
	newCar->drawY = playerY + 480. + 54.0;
	newCar->x = (double)random(topRec.x, topRec.w - 42);
	newCar->w = 42;
	newCar->h = 54;
	newCar->speedX = 0;
	newCar->speedY = 200;
	newCar->isOutRoad = 1;
	newCar->pushed = 0;
	if (newCar->type == 0)
		newCar->sprite = spriteF;
	else
		newCar->sprite = spriteE;
	carTab[i] = newCar;
}

void SpawnMissle(Missle** missleTab, Car* car, double playerY, Uint32 outlineColor, Uint32 fillColor) {
	int i = 0;
	Missle* newMissle = (Missle*)malloc(sizeof(Missle));
	while (missleTab[i] != NULL)
		i++;
	newMissle->x = car->x + (double)car->w / 2;
	newMissle->y = car->y;
	newMissle->drawY = car->drawY;
	newMissle->distance = 0;
	newMissle->range = 200;
	newMissle->w = 5;
	newMissle->h = 10;
	newMissle->speedY = 750;
	newMissle->outlineColor = outlineColor;
	newMissle->fillColor = fillColor;
	missleTab[i] = newMissle;
}
//zatrzymywanie zespawnowanego samochodu na drodze(aby nie wyjechal poza droge)
int CheckCarOnRoad(SDL_Rect drawnRoad[], int n, SDL_Rect car) {
	int newX = 0;
	int minX = 0;
	int j;
	for (j = 0; j < n; j++) {
		if (rectOverlap(drawnRoad[j], car) != 0) {
			if (car.x < drawnRoad[j].x) {
				newX = drawnRoad[j].x;
				if (minX == 0)
					minX = newX;
				else if (newX <= minX)
					minX = newX;
			}
			else if (car.x + car.w > drawnRoad[j].x + drawnRoad[j].w)
				newX = drawnRoad[j].x + drawnRoad[j].w - car.w;
			if (minX == 0)
				minX = newX;
			else if (newX <= minX)
				minX = newX;
		}
	}
	if (minX != 0)
		return minX;
	else
		return 0;
}
// rysowanie samochodow zespawnowanych
void DrawCars(SDL_Surface* screen, Car** carTab, size_t len, Car* car, double playerY, SDL_Rect drawnRoad[], int n) {
	int i, j, rv;
	int x, y, w, h;
	int newX = 0;
	SDL_Rect carRec;
	SDL_Rect carDraw;

	for (i = 0; i < len; ++i) {
		if (carTab[i] != NULL) {
			carRec.x = (int)carTab[i]->x;
			carRec.y = (int)carTab[i]->y;
			carRec.w = carTab[i]->w;
			carRec.h = carTab[i]->h;
			rv = CheckRec(carRec, (int)car->y, (int)(car->y + 480));
			if (rv == 1) {
				x = (int)carTab[i]->x;
				y = 480 - (int)(carTab[i]->drawY - playerY);
				carDraw.x = x;
				carDraw.y = y;
				carDraw.w = carRec.w;
				carDraw.h = carRec.h;
				if (carTab[i]->pushed == 0)
					newX = CheckCarOnRoad(drawnRoad, n, carDraw);
				if (newX != 0) {
					x = newX;
					carTab[i]->x = newX;
				}
				DrawSurface(screen, carTab[i]->sprite, x, y);
			}
			else if (rv == 2) {
				x = (int)carTab[i]->x;
				y = 480 - (int)(carTab[i]->drawY - playerY);
				w = carTab[i]->w;
				h = (int)carTab[i]->drawY - (int)playerY;
				SDL_Rect dest;
				dest.x = x;
				dest.y = y;
				dest.w = w;
				dest.h = h;
				SDL_Rect src;
				src.x = 0;
				src.y = 0;
				src.w = w;
				src.h = h;
				if (carTab[i]->pushed == 0)
					newX = CheckCarOnRoad(drawnRoad, n, dest);
				if (newX != 0) {
					dest.x = newX;
					carTab[i]->x = newX;
				}
				SDL_BlitSurface(carTab[i]->sprite, &src, screen, &dest);
			}
			else if (rv == 3) {
				x = (int)carTab[i]->x;
				y = 0;
				w = carTab[i]->w;
				h = carTab[i]->h - ((int)carTab[i]->drawY - ((int)playerY + 480));
				SDL_Rect dest;
				dest.x = x;
				dest.y = y;
				dest.w = w;
				dest.h = h;
				SDL_Rect src;
				src.x = x;
				src.y = carTab[i]->h - h;
				src.w = w;
				src.h = h;
				if (carTab[i]->pushed == 0)
					newX = CheckCarOnRoad(drawnRoad, n, dest);
				if (newX != 0) {
					dest.x = newX;
					carTab[i]->x = newX;
				}
				SDL_BlitSurface(carTab[i]->sprite, &src, screen, &dest);
			}
		}
	}
	return;
}
//sprawdza typ kolizji
Colision CheckColision(Car* c, Car** carTab, int len) {
	int i, colisionType;
	Colision colision;
	SDL_Rect player;
	SDL_Rect car;
	player.x = (int)c->x;
	player.y = (int)c->y + (480 - (int)c->drawY);
	player.w = c->w;
	player.h = c->h;
	for (i = 0; i < len; i++) {
		if (carTab[i] == NULL)
			continue;
		car.x = (int)carTab[i]->x;
		car.y = (int)carTab[i]->y;
		car.w = carTab[i]->w;
		car.h = carTab[i]->h;
		colisionType = rectOverlap(car, player);
		//kazda inna niz boczna kolizja
		if (colisionType == 1) {
			colision.type = 1;
			colision.car = carTab[i];
			colision.index = i;
			return colision;
		}
		//boczna kolizja
		else if (colisionType == 2) {
			colision.type = 2;
			colision.car = carTab[i];
			colision.index = i;
			return colision;
		}
	}
	colision.index = 0;
	colision.type = 0;
	colision.car = NULL;
	return colision;
}
//sprawdza czy trafil samochod
Colision CheckColisionMissle(Missle* m, Car** carTab, int len) {
	int i, colisionType;
	Colision colision;
	SDL_Rect missle;
	SDL_Rect car;
	missle.x = (int)m->x;
	missle.y = (int)m->y + (480 - (int)m->drawY);
	missle.w = m->w;
	missle.h = m->h;
	for (i = 0; i < len; i++) {
		if (carTab[i] == NULL)
			continue;
		car.x = (int)carTab[i]->x;
		car.y = (int)carTab[i]->y;
		car.w = carTab[i]->w;
		car.h = carTab[i]->h;
		colisionType = rectOverlap(car, missle);
		if (colisionType == 1) {
			colision.type = 1;
			colision.car = carTab[i];
			colision.index = i;
			return colision;
		}
	}
	colision.index = 0;
	colision.type = 0;
	colision.car = NULL;
	return colision;
}


// main
#ifdef __cplusplus
extern "C"
#endif
int main(int argc, char** argv) {
	int t1, t2, quit, frames, rc, score, speeding, recN, carsCount, missleCount, i,
		pauza, gameEnd, hitFriend, shooting, enemyKilled;
	double playerY, delta, gameTime, fpsTimer, fps, speedTime, spawnTime, tmpX, hitFriendTime, shootDelay;
	const Uint8* state;
	size_t len;
	SDL_Event event;
	SDL_Surface* screen, * charset;
	SDL_Surface* carSurface;
	SDL_Surface* carSurfaceEnemy;
	SDL_Surface* carSurfaceFriend;
	SDL_Texture* scrtex;
	SDL_Window* window;
	SDL_Renderer* renderer;
	Car car = { 2, 0, 200, 330, 42, 54, 0, 0, 0 };
	Car* carTab[100];
	Missle* missleTab[20];
	memset(missleTab, 0, 20 * sizeof(Missle*));
	memset(carTab, 0, 100 * sizeof(Car*));
	SDL_Rect drawnRoad[10];
	Colision colision;
	Colision colisionMissle;
	

	// okno konsoli nie jest widoczne, je eli chcemy zobaczy 
	// komunikaty wypisywane printf-em trzeba w opcjach:
	// project -> szablon2 properties -> Linker -> System -> Subsystem
	// zmieni  na "Console"
	// console window is not visible, to see the printf output
	// the option:
	// project -> szablon2 properties -> Linker -> System -> Subsystem
	// must be changed to "Console"
	printf("wyjscie printfa trafia do tego okienka\n");
	printf("printf output goes here\n");

	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		printf("SDL_Init error: %s\n", SDL_GetError());
		return 1;
	}

	// tryb pe noekranowy / fullscreen mode
//	rc = SDL_CreateWindowAndRenderer(0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP,
//	                                 &window, &renderer);
	rc = SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, 0,
		&window, &renderer);
	if (rc != 0) {
		SDL_Quit();
		printf("SDL_CreateWindowAndRenderer error: %s\n", SDL_GetError());
		return 1;
	};

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

	SDL_SetWindowTitle(window, "Szablon do zdania drugiego 2017");


	screen = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32,
		0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

	scrtex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		SCREEN_WIDTH, SCREEN_HEIGHT);


	// wy  czenie widoczno ci kursora myszy
	SDL_ShowCursor(SDL_DISABLE);

	// wczytanie obrazka cs8x8.bmp
	charset = SDL_LoadBMP("./cs8x8.bmp");
	if (charset == NULL) {
		printf("SDL_LoadBMP(cs8x8.bmp) error: %s\n", SDL_GetError());
		SDL_FreeSurface(screen);
		SDL_DestroyTexture(scrtex);
		SDL_DestroyWindow(window);
		SDL_DestroyRenderer(renderer);
		SDL_Quit();
		return 1;
	};
	SDL_SetColorKey(charset, true, 0x000000);

	carSurface = SDL_LoadBMP("./car.bmp");
	if (carSurface == NULL) {
		printf("SDL_LoadBMP(car.bmp) error: %s\n", SDL_GetError());
		SDL_FreeSurface(charset);
		SDL_FreeSurface(screen);
		SDL_FreeSurface(carSurface);
		SDL_DestroyTexture(scrtex);
		SDL_DestroyWindow(window);
		SDL_DestroyRenderer(renderer);
		SDL_Quit();
		return 1;
	};

	carSurfaceEnemy = SDL_LoadBMP("./carEnemy.bmp");
	if (carSurfaceEnemy == NULL) {
		printf("SDL_LoadBMP(car.bmp) error: %s\n", SDL_GetError());
		SDL_FreeSurface(charset);
		SDL_FreeSurface(screen);
		SDL_FreeSurface(carSurface);
		SDL_FreeSurface(carSurfaceEnemy);
		SDL_FreeSurface(carSurfaceFriend);
		SDL_DestroyTexture(scrtex);
		SDL_DestroyWindow(window);
		SDL_DestroyRenderer(renderer);
		SDL_Quit();
		return 1;
	};

	carSurfaceFriend = SDL_LoadBMP("./carFriend.bmp");
	if (carSurfaceFriend == NULL) {
		printf("SDL_LoadBMP(car.bmp) error: %s\n", SDL_GetError());
		SDL_FreeSurface(charset);
		SDL_FreeSurface(screen);
		SDL_FreeSurface(carSurface);
		SDL_FreeSurface(carSurfaceEnemy);
		SDL_FreeSurface(carSurfaceFriend);
		SDL_DestroyTexture(scrtex);
		SDL_DestroyWindow(window);
		SDL_DestroyRenderer(renderer);
		SDL_Quit();
		return 1;
	};

	char text[128];
	int czarny = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
	int zielony = SDL_MapRGB(screen->format, 0x00, 0xFF, 0x00);
	int czerwony = SDL_MapRGB(screen->format, 0xFF, 0x00, 0x00);
	int niebieski = SDL_MapRGB(screen->format, 0x11, 0x11, 0xCC);
	int teren = SDL_MapRGB(screen->format, 0x00, 0x50, 0x00);
	// tablica prostokatow tworzacych droge
	const SDL_Rect road[] = {
		{50, 8900, 540, 800},
		{70, 8100, 500, 400},
		{90, 7700, 460, 400},
		{110, 7300, 420, 400},
		{130, 6900, 380, 400},
		{150, 6500, 340, 400},
		{170, 6100, 300, 400},
		{190, 5700, 260, 400},
		{210, 5300, 220, 2000},
		{190, 3300, 260, 400},
		{170, 2900, 300, 400},
		{150, 2500, 340, 400},
		{130, 2100, 380, 400},
		{110, 1700, 420, 440},
		{90, 1300, 460, 400},
		{70, 900, 500, 400},
		{50, 500, 540, 500}
	};

	t1 = SDL_GetTicks();

	frames = 0;
	fpsTimer = 0;
	fps = 0;
	quit = 0;
	gameTime = 0;
	speedTime = 0;
	spawnTime = 0;
	hitFriendTime = 0;
	score = 0;
	speeding = 0;
	recN = 0;
	carsCount = 0;
	playerY = 0;
	pauza = 0;
	gameEnd = 0;
	shootDelay = 0.5;
	shooting = 0;
	colision.index = 0;
	colision.car = NULL;
	colision.type = 0;
	colisionMissle.car = NULL;
	colision.type = 0;
	enemyKilled = 0;
	hitFriend = 0;

	len = sizeof(road) / sizeof(road[1]);

	while (!quit) {

		t2 = SDL_GetTicks();

		// w tym momencie t2-t1 to czas w milisekundach,
		// jaki uplyna  od ostatniego narysowania ekranu
		// delta to ten sam czas w sekundach
		// here t2-t1 is the time in milliseconds since
		// the last screen was drawn
		// delta is the same time in seconds
		delta = (t2 - t1) * 0.001;
		t1 = t2;

		if (pauza == 0 && gameEnd == 0) {
			gameTime += delta;
			spawnTime += delta;
			shootDelay += delta;
		}
		//blokada punktow po znisczeniu sojusznika
		if (hitFriend == 1 && pauza == 0 && gameEnd == 0) {
			hitFriendTime += delta;
			if (hitFriendTime > 5.0) {
				hitFriendTime = 0;
				hitFriend = 0;
			}
		}
		// zdobywanie punktow
		if (car.isOutRoad == 0 && speeding == 1 && gameEnd == 0 && pauza == 0 && hitFriend == 0) {
			speedTime += delta;
			score = ((int)(speedTime * 4)) * 50 + enemyKilled * 500;
		}

		SDL_FillRect(screen, NULL, teren);
		DrawRectangle(screen, 0, 0, SCREEN_WIDTH / 8, SCREEN_HEIGHT, teren, teren);
		DrawRectangle(screen, (SCREEN_WIDTH / 8) * 7, 0, SCREEN_WIDTH / 8, SCREEN_HEIGHT, teren, teren);
		//wywolujemy funkcje sprawdzajace
		carsCount = CountCars(carTab);
		missleCount = CountMissles(missleTab);
		colision = CheckColision(&car, carTab, carsCount);
		//sprawdzamy dla kazdego pocisku kolizje z kazdym innym samochodem
		for (i = 0; i < missleCount; i++) {
			if (missleTab[i] != NULL) {
				colisionMissle = CheckColisionMissle(missleTab[i], carTab, carsCount);
				if (colisionMissle.type != 0 && colisionMissle.car != NULL) {
					if (colisionMissle.car->type == 0)
						hitFriend = 1;
					if (colisionMissle.car->type == 1)
						enemyKilled++;
					carTab[colision.index] = NULL;
					free(colisionMissle.car);
					colisionMissle.car = NULL;
				}
				colision.index = 0;
				colisionMissle.car = NULL;
				colisionMissle.type = 0;
			}
		}


		tmpX = car.x;
		car.x += delta * car.speedX;
		car.y += delta * car.speedY;
		playerY += delta * car.speedY;

		if (car.x > (640 - car.w) || car.x < 0) {
			gameEnd = 1;
			car.speedX = 0;
			car.speedY = 0;
			int i;
			for (i = 0; i < len; i++) {
				if (carTab[i] == NULL)
					continue;
				carTab[i]->speedX = 0;
				carTab[i]->speedY = 0;
			}
		}

		//w ktora strona ma poleciec samochod
		if (colision.type == 2) {
			if (colision.car->pushed != 1) {
				if (car.x - tmpX > 0) {// prawo
					colision.car->speedX = 250;
					colision.car->pushed = 1;
					if (colision.car->type == 0)
						hitFriend = 1;
					if (colision.car->type == 1)
						enemyKilled++;
				}
				else if (car.x - tmpX < 0) {//lewo
					colision.car->speedX = -250;
					colision.car->pushed = 1;
					if (colision.car->type == 0)
						hitFriend = 1;
					if (colision.car->type == 1)
						enemyKilled++;
				}

			}
		}
		//kolizja inna niz boczna = koniec gry
		else if (colision.type == 1) {
			if (colision.car->pushed != 1) {
				gameEnd = 1;
				car.speedX = 0;
				car.speedY = 0;
				int i;
				for (i = 0; i < len; i++) {
					if (carTab[i] == NULL)
						continue;
					carTab[i]->speedX = 0;
					carTab[i]->speedY = 0;
				}
			}
		}

		//reset kolizji na brak kolizji
		colision.index = 0;
		colision.type = 0;
		colision.car = NULL;
		//zapetlanie drogi
		if (playerY > 8420) {
			playerY = (double)((int)car.y % 8420);
			for (i = 0; i < carsCount && carTab[i] != NULL; i++) {
				carTab[i]->drawY = (double)((int)(carTab[i]->drawY) % 8420);
			}
		}
		//usuwanie samochodow
		for (i = 0; i < carsCount; i++) {
			if (carTab[i] != NULL) {
				if (abs((int)(carTab[i]->y - car.y)) > 3000) {//wartosc bezwzgledna(abs)
					free(carTab[i]);
					carTab[i] = NULL;
					continue;
				}
				carTab[i]->y += delta * carTab[i]->speedY;
				carTab[i]->x += delta * carTab[i]->speedX;
				carTab[i]->drawY += delta * carTab[i]->speedY;
				if (carTab[i]->x < 0 || carTab[i]->x >(640 - carTab[i]->w)) {

					carTab[i] = NULL;
					continue;
				}
				if (carTab[i]->drawY > 8900)
					carTab[i]->drawY = (double)((int)(carTab[i]->drawY) % 8420);
			}
		}

		recN = DrawRoad(screen, road, len, &car, playerY, drawnRoad, czarny, czarny);//dlugosc tablicy tych narysowanych prostokatow
		DrawSurface(screen, carSurface, (int)car.x, car.drawY);//rysuje nasz samochod
		DrawCars(screen, carTab, carsCount, &car, playerY, drawnRoad, recN);//rysuje inne samochody

		for (i = 0; i < missleCount; i++) {
			if (missleTab[i] != NULL) {
				if (missleTab[i]->distance > missleTab[i]->range) {//usuwanie pocisku
					free(missleTab[i]);
					missleTab[i] = NULL;
					continue;
				}
				//inkrementacja pociskow
				missleTab[i]->y += delta * missleTab[i]->speedY;
				missleTab[i]->drawY -= delta * missleTab[i]->speedY;
				missleTab[i]->distance += delta * missleTab[i]->speedY;
				DrawRectangle(screen, (int)missleTab[i]->x, (int)(missleTab[i]->drawY), missleTab[i]->w,//rysowanie pocisku
					missleTab[i]->h, missleTab[i]->outlineColor, missleTab[i]->fillColor);
			}
		}

		if (spawnTime >= 5.0) {//spawnowanie samochodu
			SpawnCar(carTab, &car, playerY, carSurfaceFriend, carSurfaceEnemy, FindTopRec(drawnRoad, recN));
			spawnTime = 0;
		}

		if (shootDelay >= 0.5 && shooting == 1) {//strzelanie co 0,5sek
			SpawnMissle(missleTab, &car, playerY, czerwony, czerwony);
			shootDelay = 0;
		}

		fpsTimer += delta;
		if (fpsTimer > 0.5) {
			fps = frames * 2;
			frames = 0;
			fpsTimer -= 0.5;
		};

		// tekst informacyjny / info text
		DrawRectangle(screen, 4, 4, SCREEN_WIDTH - 8, 36, czerwony, niebieski);
		//            "template for the second project, elapsed time = %.1lf s  %.0lf frames / s"
		DrawString(screen, 10, 10, "Przemek Debek 193378", charset);
		DrawString(screen, 485, 455, "esc n p spacja ", charset);
		DrawString(screen, 485, 465, "a b c d e f h i j k ", charset);

		sprintf(text, "Wynik: %d", score);
		DrawString(screen, screen->w / 2 - strlen(text) * 8 / 2, 10, text, charset);

		memset(text, 0, strlen(text));
		sprintf(text, "Czas: %.1lf", gameTime);
		DrawString(screen, screen->w / 2 - strlen(text) * 8 / 2, 26, text, charset);

		memset(text, 0, strlen(text));
		sprintf(text, "Fps: %.0lf", fps);
		DrawString(screen, 550, 26, text, charset);

		if (gameEnd == 1) {
			DrawString(screen, screen->w / 2 - strlen(text) * 8 / 2,
				screen->h / 2 - strlen(text) * 8 / 2, "Koniec gry", charset);
		}

		SDL_UpdateTexture(scrtex, NULL, screen->pixels, screen->pitch);
		//		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, scrtex, NULL, NULL);
		SDL_RenderPresent(renderer);

		// obs uga zdarze  (o ile jakie  zasz y) / handling of events (if there were any)
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_ESCAPE) quit = 1;
				else if (event.key.keysym.sym == SDLK_SPACE && !pauza && !gameEnd) {
					shooting = 1;
				}
				else if (event.key.keysym.sym == SDLK_LEFT && !pauza && !gameEnd) {
					car.speedX = -400;
				}
				else if (event.key.keysym.sym == SDLK_RIGHT && !pauza && !gameEnd) {
					car.speedX = 400;
				}
				else if (event.key.keysym.sym == SDLK_UP && !pauza && !gameEnd) {
					car.speedY = 500;
					speeding = 1;
				}
				else if (event.key.keysym.sym == SDLK_n && !pauza) {
					gameEnd = 0;
					car.y = 0;
					playerY = 0;
					car.x = SCREEN_WIDTH / 2 - car.w / 2;
					gameTime = 0;
					score = 0;
					speedTime = 0;
					spawnTime = 0;
					shootDelay = 0.5;
					enemyKilled = 0;
					memset(carTab, 0, 100 * sizeof(Car*));
				}
				else if (event.key.keysym.sym == SDLK_p && !gameEnd) {
					if (pauza == 0) {
						pauza = 1;
						car.speedX = 0;
						car.speedY = 0;
						int i;
						for (i = 0; i < len; i++) {
							if (carTab[i] == NULL)
								continue;
							carTab[i]->speedX = 0;
							carTab[i]->speedY = 0;
						}
					}
					else {
						pauza = 0;
						int i;
						for (i = 0; i < len; i++) {
							if (carTab[i] == NULL)
								continue;
							carTab[i]->speedX = 0;
							carTab[i]->speedY = 200;
						}
					}

				}
				break;
			case SDL_QUIT:
				quit = 1;
				break;
			};
		};

		state = SDL_GetKeyboardState(NULL);

		if (state[SDL_SCANCODE_LEFT] == 0 && state[SDL_SCANCODE_RIGHT] == 0 && !pauza) {
			car.speedX = 0;
		}
		if (state[SDL_SCANCODE_SPACE] == 0 && !pauza) {
			shooting = 0;
			shootDelay = 0.5;
		}
		if (state[SDL_SCANCODE_UP] == 0 && !pauza && !gameEnd) {
			car.speedY = 50;
			speeding = 0;
		}
		frames++;
	}

	// zwolnienie powierzchni / freeing all surfaces
	SDL_FreeSurface(charset);
	SDL_FreeSurface(screen);
	SDL_DestroyTexture(scrtex);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	SDL_Quit();
	return 0;
};
