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