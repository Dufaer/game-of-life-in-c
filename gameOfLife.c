/* Program gameOfLife.c */
/* This is a litte implementation of John Conway's Game of Life cellular automaton.
 *
 * A Game - a pair of 2-dimensional char arrays is created by createGame.
 * Each 2-dimensional char array is a Grid. Grid size is limited by LLONG_MAX in the x-component and by LLONG_MAX * CHAR_BIT in the y-component.
 * Each bit represents a single cellof the automaton.
 * A torus "wrap-around" topology is optional (GOL__OOBR__TORUS).
 * 
 * iterateGame updates the state according to Game of Life's rules.
 * It reads from one Grid and writes into the other, then switches. That way, no new memory needs to be allocated.
 *
 * printAndIterateGameLoop showcases the evolution a single game in an endless loop in the standard output.
 *
 * This program works on Windows and Linux.
 * Compile in linux with: gcc -o gameOfLife gameOfLife.c -std=gnu11
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define GOL__CELL_STATE__OFF 0
#define GOL__CELL_STATE__ON 1
#define GOL__CELL_STATE__INVALID 2

#define GOL__OOBR__ALL_OFF 0
#define GOL__OOBR__ALL_ON 1
#define GOL__OOBR__TORUS 2



typedef char ErrorChar;

typedef char CellState;

typedef struct Grid_ {
	char **origin;
	long long gridSizeX;
	long long gridSizeY;
	size_t arraySizeX;
	size_t arraySizeY;
	char outOfBoundsRule; //  GOL__OOBR__ALL_OFF, GOL__OOBR__ALL_ON, or GOL__OOBR__TORUS
} Grid;

typedef struct Game_ {
	Grid gridA;
	Grid gridB;
	Grid *currentGridPtr;
} Game;

typedef struct PrintOptions_ {
	char signForOff;
	char signForOn;
} PrintOptions;

typedef struct CellIndex_ {
	char *storageCharPtr;
	char bitIndex;
} CellIndex;



/* Grid - create & destroy */
Grid *createGrid( long long gridSizeX, long long gridSizeY, char outOfBoundsRule );
void destroyGrid( Grid *oldGridPtr );

/* Grid - getter and setter */
CellIndex selsectCell( Grid *gridPtr, long long x, long long y );
CellState getCell( Grid *gridPtr, long long x, long long y );
ErrorChar setCell( Grid *gridPtr, long long x, long long y, CellState newState );

/* Grid - print */
void printGrid( Grid *gridPtr, PrintOptions *optionsPtr );
void printRow( Grid *gridPtr, size_t rowIndex, PrintOptions *optionsPtr );
void printAllInChar( char storageChar, PrintOptions *options );
ErrorChar printOneInChar( char storageChar, char bitIndex, PrintOptions *options );

/* Grid - miscellaneous */
void randomizeGrid( Grid *gridPtr );


/* Game - create & destroy */
Game *createGame( long long gridSizeX, long long gridSizeY, char outOfBoundsRule );
void destroyGame( Game *oldGamePtr );

/* Game - miscellaneous */
void printGame( Game * gamePtr, PrintOptions *optionsPtr );
void iterateGame( Game * gamePtr );
void randomizeGame( Game * gamePtr );
void printAndIterateGameLoop( Game * gamePtr, PrintOptions *optionsPtr, unsigned int sleepInMilliseconds );


/* Moludo functions */
lldiv_t lldivGreater ( long long dividend, long long divisor );
lldiv_t lldivPositive ( long long dividend, long long divisor );


/* Demos */
void randomGameDemo();
void gliderGunDemo();


/* Cross-platform */ // Used only for printAndIterateGameLoop and the demos.
void clearCmd();
#ifdef _WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#define Sleep(x) usleep((x)*1000)
#endif



void main() {
	randomGameDemo();
	// gliderGunDemo();
}



/* Grid - create & destroy */

/* Creates a Grid - a 2-dimensional char array. Allocates the necessary memory. Returns a pointer to it, if successful. Retruns a NULL pointer otherwise. */
Grid *createGrid( long long gridSizeX, long long gridSizeY, char outOfBoundsRule ) {
	bool error = false;
	
	char **origin;
	size_t arraySizeX;
	size_t arraySizeY;
	
	Grid *newGridPtr = NULL;
	
	if ( gridSizeX < 0 || gridSizeY < 0 ) { // grid size must be positive
		fprintf( stderr, "ERROR: ( gridSizeX, gridSizeX ) == ( %d, %d ) is invalid. Grid size must be positive.\n", gridSizeX, gridSizeY );
		error = true;
	} else {
		newGridPtr = (Grid *) malloc( sizeof( Grid ) );
		if ( newGridPtr == NULL ) {
			error = true;
		} else {
			arraySizeX = (size_t) gridSizeX;
			arraySizeY = (size_t) lldivGreater( gridSizeY, sizeof( char ) ).quot;
			origin = (char **) calloc( arraySizeX, sizeof( char * ) );
			if ( origin == NULL ) {
				error = true;
				free( newGridPtr );
				newGridPtr = NULL;
			} else {
				size_t i = 0;
				while ( error == false && i < arraySizeX ) {
					origin[i] = (char *) calloc( arraySizeY, sizeof( char ) );
					if ( origin[i] == NULL ) {
						error = true;
					} else {
						++i;
					}
				}
				/* Rollback: */
				if ( error == true ) {
					for ( size_t toFreeIndex = 0 ; toFreeIndex < i ; ++toFreeIndex ) {
						free( origin[toFreeIndex] );
					}
					free( origin );
					free( newGridPtr );
					newGridPtr = NULL;
				}
			}
		}
		if ( error == true ) { // malloc or calloc failure
			fprintf( stderr, "ERROR: Could not allocate memory to create grid with dimensions %d by %d.\n", gridSizeX, gridSizeY );
		}
	}
	if ( error == false ) {
		newGridPtr->origin = origin;
		newGridPtr->gridSizeX = gridSizeX;
		newGridPtr->gridSizeY = gridSizeY;
		newGridPtr->arraySizeX = arraySizeX;
		newGridPtr->arraySizeY = arraySizeY;
		newGridPtr->outOfBoundsRule = outOfBoundsRule;
	}

	return newGridPtr;
}

/* Destroys the Grid pointed at by the oldGridPtr. Frees the memory. */
void destroyGrid( Grid *oldGridPtr ) {
	char **origin = oldGridPtr->origin;
	size_t arraySizeX = oldGridPtr->arraySizeX;
	
	for ( long long i = 0; i < arraySizeX ; ++i ) {
		free( origin[i] );
	}
	free( origin );
	free( oldGridPtr );
}


/* Grid - getter and Setter */

/* Retruns the CellIndex of a cell. This value is used by getCell and setCell. */
CellIndex selsectCell( Grid *gridPtr, long long x, long long y ) {
	long long gridSizeX = gridPtr->gridSizeX;
	long long gridSizeY = gridPtr->gridSizeY;
	
	CellIndex targetIndex;
	
	if ( x >= 0 && y >= 0 && x < gridSizeX && y < gridSizeY ) {
		size_t i = (size_t) x;
		
		lldiv_t division = lldiv( y, sizeof( char ) );
		size_t j = (size_t) division.quot;
		
		targetIndex.storageCharPtr = &(gridPtr->origin[i][j]);
		targetIndex.bitIndex = (char) division.rem;
		
	} else {
		if ( gridPtr->outOfBoundsRule == GOL__OOBR__TORUS ) { // torus topology
			long long newX = lldivPositive( x , gridSizeX ).rem;
			long long newY = lldivPositive( y , gridSizeY ).rem;
			
			targetIndex = selsectCell( gridPtr, newX, newY );
			
			// TEST: x = gridSizeX => x = 0;
			// TEST: x = 2 * gridSizeX => x = 0;
			// TEST: x = -1 => x = gridSizeX;
			
		} else { // Out-of-bounds cell is not selectable. getCell uses outOfBoundsRule to determine its value.
			targetIndex.storageCharPtr = NULL;
			targetIndex.bitIndex = (char) sizeof( char );
		}
	}
	
	return targetIndex;
}

/* Reads a single cell. This corresponds to a single bit in the 2-dimensional char array that is a Grid. Returns GOL__OOBR__ALL_OFF, GOL__OOBR__ALL_ON, or GOL__OOBR__ALL_INVALID. */
CellState getCell( Grid *gridPtr, long long x, long long y ) {
	CellIndex targetIndex = selsectCell( gridPtr, x, y);
	char *storageCharPtr = targetIndex.storageCharPtr;
	char bitIndex = targetIndex.bitIndex;
	
	char outOfBoundsRule = gridPtr->outOfBoundsRule;
	
	CellState state;
	
	if ( storageCharPtr == NULL ) {
		if ( bitIndex != (char) sizeof( char ) ) { // invalid bitIndex // This shouldn't be possible.
			fprintf( stderr, "ERROR: bitIndex == %d is invalid for storageCharPtr == NULL. Valid value is only %d. selectCell for buggy? Invalid Grid?\n", bitIndex, (char) sizeof( char ) );
			state = GOL__CELL_STATE__INVALID;
		} else {
			if ( outOfBoundsRule == GOL__OOBR__ALL_OFF ) {
				state = GOL__CELL_STATE__OFF;
			} else if ( outOfBoundsRule == GOL__CELL_STATE__ON ) {
				state = GOL__CELL_STATE__ON;
			} else {  // invalid outOfBoundsRule; If outOfBoundsRule == GOL__OOBR__TORUS, we never get here.
				fprintf( stderr, "ERROR: outOfBoundsRule == %d is invalid. Valid values are only %d, %d and %d.\n", outOfBoundsRule, GOL__OOBR__ALL_OFF, GOL__CELL_STATE__ON, GOL__OOBR__TORUS );
				state = GOL__CELL_STATE__INVALID;
			}
		}
	} else {
		if ( ( *storageCharPtr &&  (char) ( 1 << bitIndex ) ) == 0 ) {
			state = GOL__CELL_STATE__OFF;
		} else {
			state = GOL__CELL_STATE__ON;
		}
	}
	
	return state;
}

/* Writes a single cell. This corresponds to a single bit in the 2-dimensional char array that is a Grid. Returns 0 on success; > 0 on error. */
ErrorChar setCell( Grid *gridPtr, long long x, long long y, CellState newState ) {
	CellIndex targetIndex = selsectCell( gridPtr, x, y);
	char *storageCharPtr = targetIndex.storageCharPtr;
	char bitIndex = targetIndex.bitIndex;
	
	ErrorChar error = 0;
	
	if ( bitIndex == (char) sizeof( char ) ) { // Cell is out-of-bounds and thus not settable.
		error = 1;
		fprintf( stderr, "ERROR: bitIndex == %d is invalid. Cell with ( x, y ) == ( %d, %d ) is out-of-bounds and thus not settable.\n", bitIndex, x, y );
	} else {
		if ( newState == GOL__CELL_STATE__OFF ) {
			*storageCharPtr &= !( (char) ( 1 << bitIndex ) );
		} else if ( newState == GOL__CELL_STATE__ON ) {
			*storageCharPtr |= (char) ( 1 << bitIndex );
		} else { // invalid newState
			error = 2;
			fprintf( stderr, "ERROR: newState == %d is invalid. Valid values are only %d and %d.\n", newState, GOL__CELL_STATE__OFF, GOL__CELL_STATE__ON );
		}
	}
	
	return error;
}


/* Grid - print */

/* Prints the Grid to stdout. */
void printGrid( Grid *gridPtr, PrintOptions *optionsPtr ) {
	size_t arraySizeX = gridPtr->arraySizeX;
	
	putc( '\n', stdout );
	for ( size_t i = 0; i < arraySizeX; ++i ) {
		printRow( gridPtr, i, optionsPtr );
	}
	putc( '\n', stdout );
}

/* Prints a whole row of cells in sequence to stdout. */
void printRow( Grid *gridPtr, size_t rowIndex, PrintOptions *optionsPtr ) {
	char *rowOrigin = gridPtr->origin[rowIndex];
	
	lldiv_t division = lldiv( gridPtr->gridSizeY, sizeof( char ) );
	size_t quotient = (size_t) division.quot;
	char remainder = (char) division.rem;
	
	for ( size_t j = 0; j < quotient; ++j ) {
		printAllInChar( rowOrigin[j], optionsPtr );
	}
	for ( char bitIndex = 0; bitIndex < remainder; ++bitIndex ) {
		printOneInChar( rowOrigin[quotient], bitIndex, optionsPtr );
	}
	putc( '\n', stdout );
}

/* Prints all cells in a char in sequence to stdout. */
void printAllInChar( char storageChar, PrintOptions *options ) {
	char signForOff = options->signForOff;
	char signForOn = options->signForOn;
	
	char sizeOfChar = sizeof( char );
	
	for ( char bitIndex = 0; bitIndex < sizeOfChar; ++bitIndex ) {
		if ( storageChar && 1 << bitIndex ) {
			putc( signForOn, stdout );
		} else {
			putc( signForOff, stdout );
		}		
	}
}

/* Prints a single cell in a char to stdout. Returns 0 on success; > 0 on invalid bitIndex. */
ErrorChar printOneInChar( char storageChar, char bitIndex, PrintOptions *options ) {
	ErrorChar error = 0;
	
	char signForOff = options->signForOff;
	char signForOn = options->signForOn;
	
	if ( bitIndex >= sizeof( char ) ) {
		error = 1;
	} else {
		if ( storageChar && 1 << bitIndex ) {
			putc( signForOff, stdout );
		} else {
			putc( signForOn, stdout );
		}		
	}
	return error;
}


/* Grid - miscellaneous */

/* Randomize each cell of the grid individually. Not very efficient. The distribution is questionable. */
void randomizeGrid( Grid *gridPtr ) {
	long long  gridSizeX = gridPtr->gridSizeX;
	long long  gridSizeY = gridPtr->gridSizeY;
	for ( long long i = 0; i < gridSizeX; ++i ) {
		for ( long long j = 0; j < gridSizeY; ++j ) {
			if ( rand() % 2 ) {
				setCell( gridPtr, i, j, GOL__CELL_STATE__OFF );
			} else {
				setCell( gridPtr, i, j, GOL__CELL_STATE__ON );
			}
		}
	}
}


/* Game - create & destroy */

/* Creates a Game - a pair of Grid of equal size with a currentGridPtr. Allocates the necessary memory. Returns a pointer to it, if successful. Returns a NULL pointer otherwise. */
Game *createGame( long long gridSizeX, long long gridSizeY, char outOfBoundsRule ) {
	bool error = false;
	
	Grid *gridAPtr;
	Grid *gridBPtr;
	
	Game *newGamePtr = NULL;
	
	newGamePtr = (Game *) malloc( sizeof( Game ) );
	if ( newGamePtr == NULL ){
		error = true;
	} else {
		gridAPtr = createGrid( gridSizeX, gridSizeY, outOfBoundsRule );
		if ( gridAPtr == NULL ) {
			free ( newGamePtr );
			error = true;
		} else {
			gridBPtr = createGrid( gridSizeX, gridSizeY, outOfBoundsRule );
			if ( gridBPtr == NULL ) {
				free ( gridAPtr );
				free ( newGamePtr );
				error = true;
			}
		}
	}
	if ( error == false ) {
		newGamePtr->gridA = *gridAPtr;
		newGamePtr->gridB = *gridBPtr;
		newGamePtr->currentGridPtr = &(newGamePtr->gridA);
	}
	
	return newGamePtr;
}

/* Destroys the Game pointed at by the oldGamePtr. Frees the memory. */
void destroyGame( Game *oldGamePtr ) {
	 destroyGrid( &(oldGamePtr->gridA) );
	 destroyGrid( &(oldGamePtr->gridB) );
	 free( oldGamePtr );
}


/* Game - miscellaneous */

/* One iteration of the Game pointed at by the gamePtr according to the rules of John Conway's Game of Life. */
void iterateGame( Game * gamePtr ) {
	bool error = false;
	
	Grid *gridAPtr = &(gamePtr->gridA);
	Grid *gridBPtr = &(gamePtr->gridB);
	Grid *srcGridPtr = gamePtr->currentGridPtr;
	Grid *trgGridPtr = NULL;
	
	if ( srcGridPtr == gridAPtr ) {
		trgGridPtr = gamePtr->currentGridPtr = gridBPtr;
	} else if ( srcGridPtr == gridBPtr ) {
		trgGridPtr = gamePtr->currentGridPtr = gridAPtr;
	} else {
		fprintf( stderr, "ERROR: currentGridPtr == %p is invalid. currentGridPtr must be either %p or %p.\n", srcGridPtr, gridAPtr, gridBPtr );
		error = true;
	}
	if ( error == false ) {
		long long  gridSizeX = srcGridPtr->gridSizeX;
		long long  gridSizeY = srcGridPtr->gridSizeY;
		for ( long long i = 0; i < gridSizeX; ++i ) {
			for ( long long j = 0; j < gridSizeY; ++j ) {
				char neighbors = 0;
				neighbors +=
					getCell( srcGridPtr, i-1, j-1 ) +
					getCell( srcGridPtr, i-1, j   ) +
					getCell( srcGridPtr, i-1, j+1 ) +
					getCell( srcGridPtr, i  , j-1 ) +
					getCell( srcGridPtr, i  , j+1 ) +
					getCell( srcGridPtr, i+1, j-1 ) +
					getCell( srcGridPtr, i+1, j   ) +
					getCell( srcGridPtr, i+1, j+1 ); // At the current state, this assumes that 0 is off and 1 is on. I will make this indepent of the values of magic numbers, in a later revision. // TODO
				/* Rules of the Game of Life */
				if ( getCell( srcGridPtr, i, j ) == GOL__CELL_STATE__OFF ){
					if ( neighbors == 3) {
						setCell( trgGridPtr, i, j, GOL__CELL_STATE__ON );
					} else {
						setCell( trgGridPtr, i, j, GOL__CELL_STATE__OFF );
					}
				} else { // cell starts dead
					if ( neighbors < 2 || neighbors > 3 ) {
						setCell( trgGridPtr, i, j, GOL__CELL_STATE__OFF );
					} else {
						setCell( trgGridPtr, i, j, GOL__CELL_STATE__ON );
					}
				}
			}
		}
	}
}

/* Prints the current state of a Game into stdout. */
void printGame( Game * gamePtr, PrintOptions *optionsPtr ) {
	printGrid( gamePtr->currentGridPtr, optionsPtr );
}

/* Randomizes all cells in a Game. */
void randomizeGame( Game * gamePtr ){
	randomizeGrid( gamePtr->currentGridPtr );
}

/* An endless loop to showcase the evolution of a Game of Life Game. Prints to stdout. */
void printAndIterateGameLoop( Game * gamePtr, PrintOptions *optionsPtr, unsigned int sleepInMilliseconds ) {
	bool running = true;
	
	fflush( stdout );
	clearCmd();
	
	printf( "GAME OF LIFE\n" );
	putc( '\n', stdout );
	
	while ( running == true ) {
		printGame( gamePtr, optionsPtr );		
		iterateGame( gamePtr );
		
		fflush( stdout );
		Sleep( sleepInMilliseconds );
		clearCmd();
	}
}


/* Moludo functions */

/* Pseudo-modulo operation. divisor * quotient >= dividend for positive arguments. */
lldiv_t lldivGreater ( long long dividend, long long divisor ) {
	lldiv_t greater;
	lldiv_t normal = lldiv( dividend, divisor );
	if ( normal.rem == 0) {
		greater.quot = normal.quot;
		greater.rem = normal.rem;
	} else {
		if ( ( dividend >= 0 && divisor >= 0 ) ||
			( dividend < 0 && divisor < 0 ) ) {
			greater.quot = normal.quot + 1;
			greater.rem = normal.rem - divisor;
		} else {
			greater.quot = normal.quot - 1;
			greater.rem = normal.rem + divisor;	
		}
	}
	return greater;
};

/* Modulo operation with nonnegative remainder. */
lldiv_t lldivPositive ( long long dividend, long long divisor ) {
	lldiv_t positive;
	lldiv_t normal = lldiv( dividend, divisor );
	if ( normal.rem >= 0 ) {
		positive = normal;
	} else {
		positive.quot = normal.quot - 1;
		positive.rem = normal.rem + divisor;
	}
	return positive;
}


/* Demos */
/* An endless loop to showcase the evolution of random 20 x 40 torusoid Game of Life Game. Prints to stdout. */
void randomGameDemo() {
	long long myGridSizeX = 20;
	long long myGridSizeY = 40;
	char myOutOfBoundsRule = GOL__OOBR__TORUS;
	
	Game *randomGame = createGame( myGridSizeX, myGridSizeY, myOutOfBoundsRule );

	randomizeGame( randomGame );
	
	PrintOptions demoOptions = {'.', 'O'};
	unsigned int sleepInMilliseconds = 100;

	printAndIterateGameLoop( randomGame, &demoOptions, sleepInMilliseconds );
}

/* An endless loop to showcase the cyclic behaviour of a Game of Life glider gun. Prints to stdout. */
void gliderGunDemo() {
	long long myGridSizeX = 20;
	long long myGridSizeY = 40;
	char myOutOfBoundsRule = GOL__OOBR__ALL_OFF;
	
	Game *gliderGunGame = createGame( myGridSizeX, myGridSizeY, myOutOfBoundsRule );

	/* Setting the glider gun point by point */
	/* X == 1 */
	setCell( gliderGunGame->currentGridPtr, 1, 25, GOL__CELL_STATE__ON );
	/* X == 2 */
	setCell( gliderGunGame->currentGridPtr, 2, 23, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 2, 25, GOL__CELL_STATE__ON );
	/* X == 3 */
	setCell( gliderGunGame->currentGridPtr, 3, 13, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 3, 14, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 3, 21, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 3, 22, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 3, 35, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 3, 36, GOL__CELL_STATE__ON );
	/* X == 4 */
	setCell( gliderGunGame->currentGridPtr, 4, 12, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 4, 16, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 4, 21, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 4, 22, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 4, 35, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 4, 36, GOL__CELL_STATE__ON );
	/* X == 5 */
	setCell( gliderGunGame->currentGridPtr, 5,  1, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 5,  2, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 5, 11, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 5, 17, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 5, 21, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 5, 22, GOL__CELL_STATE__ON );
	/* X == 6 */
	setCell( gliderGunGame->currentGridPtr, 6,  1, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 6,  2, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 6, 11, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 6, 15, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 6, 17, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 6, 18, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 6, 23, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 6, 25, GOL__CELL_STATE__ON );
	/* X == 7 */
	setCell( gliderGunGame->currentGridPtr, 7, 11, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 7, 11, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 7, 17, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 7, 25, GOL__CELL_STATE__ON );
	/* X == 8 */
	setCell( gliderGunGame->currentGridPtr, 8, 12, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 8, 16, GOL__CELL_STATE__ON );
	/* X == 9 */
	setCell( gliderGunGame->currentGridPtr, 9, 13, GOL__CELL_STATE__ON );
	setCell( gliderGunGame->currentGridPtr, 9, 14, GOL__CELL_STATE__ON );
	
	PrintOptions demoOptions = {'.', 'O'};
	unsigned int sleepInMilliseconds = 100;

	printAndIterateGameLoop( gliderGunGame, &demoOptions, sleepInMilliseconds );
}


/* Cross-platform */

/* Cross-platform clear command line function. Used only for printAndIterateGameLoop and the demos.*/
void clearCmd() {
#ifdef LINUX
    system( "clear" );
#endif
#ifdef WINDOWS
	system( "cls" );
#endif
}