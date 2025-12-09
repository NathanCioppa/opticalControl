
typedef struct CD CD;
typedef struct CDText CDText;
typedef struct Track Track;

struct CD {
	CDText *text;
	int tracks;
};

struct Track {
	char *name;
	char *artist;
};





