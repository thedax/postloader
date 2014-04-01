#define SPOTSMAX 20

typedef struct 
	{
	int id;		// Look up to application
	s_grlib_icon ico;
	}
s_spot;

typedef struct 
	{
	s_spot spots[SPOTSMAX];
	
	int spotsIdx;
	int spotsXpage;
	int spotsXline;
	}
s_gui;

extern s_gui gui;

void gui_Init (void);
void gui_Clean (void);

int ChooseDPadReturnMode (u32 btn);
int Menu_SelectBrowsingMode (void);
int GoToPage (int page, int pageMax);

int DrawTopBar (int *visibleflag, int *browserRet, u32 *btn, int *closed);
int DrawBottomBar (int *visibleflag, u32 *btn, int *closed);

void DrawInfoWindow (f32 y, char *s1, char *s2, char *s3);