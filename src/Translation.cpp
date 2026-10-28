#include "stdafx.h"
#include <stdio.h>
#include "UI.h"
//#include "Objects.h"
#include "Dispatch.h"
#include "CSB.h"


typedef char *pCH;
typedef pCH *ppCH;


class XTABL
{
  int m_numXlate[32];  // indexed on first character & 0x1f
  ppCH m_english[32];
  ppCH m_language[32];
public:
  void Clear(void);
  XTABL(void);
  ~XTABL(void);
  void AddTranslation(const char *s1, const char *s2, int numLine);
  const char *Translate(const char *text);
  int EscapeCopy(char *dest, const char *src, int len);
};

XTABL::XTABL(void)
{
  int i;
  for (i=0; i<32; i++)
  {
    m_numXlate[i] = 0;
    m_english[i] = NULL;
    m_language[i] = NULL;
  };
}

XTABL::~XTABL(void)
{
  Clear();
}

void XTABL::Clear(void)
{
  int i;
  for (i=0; i<32; i++)
  {
    int j;
    for (j=0; j<m_numXlate[i]; j++)
    {
      if (m_english[i][j] != NULL) free(m_english[i][j]);
      m_english[i][j] = NULL;
      if (m_language[i][j] != NULL) free(m_language[i][j]);
      m_language[i][j] = 0;
    };
    if (m_english[i]  != NULL) free(m_english[i]);
    m_english[i] = NULL;
    if (m_language[i] != NULL) free(m_language[i]);
    m_language[i] = NULL;
    m_numXlate[i] = 0;
  };
}

static char* removeAccented( char* str ) {
    char *p = str;
    if (!strncmp(str,"Sant",4))
	printf("yes\n");
    while ( (*p)!=0 ) {
        const char*
        //   "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ"
        tr = "AAAAAAECEEEEIIIIDNOOOOOx0UUUUYPsaaaaaaeceeeeiiiiOnooooo/0uuuuypy";
        unsigned char ch = (*p);
        if ( ch ==0xc3 ) {
	    memmove(p, p+1, strlen(p));
	    ch = *p;
            (*p) = tr[ ch-0x80 ];
        }
        ++p; // http://stackoverflow.com/questions/14094621/
	     // Actually the code in stackoverflow was bad, it didn't include the memmove, but it gave me a good idea on how to do it...
	     // the guy who posted that probably didn't test it!
    }
    return str;
}

int XTABL::EscapeCopy(char *dest, const char *src, int len)
{
  // Change \n to 0x0A
  int i, j;
  for (i=0, j=0; j<len; i++, j++)
  {
    dest[j] = src[i];
    if (src[i] == '\\')
    {
      if (i < len-1)
      {
        if (src[i+1] == 'n')
        {
          dest[j] = 0x0A;
          i++;
          len--;
        }
      }
    }
  }
  return len;
}

void XTABL::AddTranslation(const char *s1, const char *s2, int numLine)
{
  char *S1 = strdup(s1);
  if (S1 == NULL) return;
  char *S2 = strdup(s2);
  if (S2 == NULL) {free(S1); return;};
  i32 len1 = EscapeCopy(S1, s1, strlen(s1)+1);
  EscapeCopy(S2, s2, strlen(s2)+1);
  // The translated text should be uppercase and without accents for now
  // hopefully it will be fixed one day... !
  int idx = S1[0];
  if (len1 > 1) idx = (idx + S1[1]);
  idx &= 0x1f;

  void *newmem;
  newmem = realloc(m_english[idx], (m_numXlate[idx]+1)*sizeof(m_english[idx][0]));
  if (newmem == NULL)
  {
    free(S1); free(S2); return;
  };
  m_english[idx] = (ppCH)newmem;
  m_english[idx][m_numXlate[idx]] = NULL;
  newmem = realloc(m_language[idx], (m_numXlate[idx]+1)*sizeof(m_language[idx][0]));
  if (newmem == NULL)
  {
    free(S1); free(S2); return;
  };
  m_language[idx] = (ppCH)newmem;
  m_language[idx][m_numXlate[idx]] = NULL;
  m_numXlate[idx]++;
  int idx2 = m_numXlate[idx]-1;
  if (!experimental_overlay && (numLine < 182 || numLine > 213) && (numLine < 447 || numLine > 449)) {
      removeAccented(S2);
      SDL_strupr(S2);
  }
  m_english [idx][idx2] = S1;
  m_language[idx][idx2] = S2;
}

XTABL xlate;

// Whether a translation table was loaded: without one every lookup "misses" and the
// message below would spam English sessions, where the fallback text is already correct.
static bool translationLoaded = false;


struct AutoFree
{
  char *buf;
  int size;
  AutoFree(void){size=200000; buf = (char *)malloc(size);};
  ~AutoFree(void){free(buf);};
};

static int myfgets(char *buff, int size, FILE *f) {
    *buff = 0;
    fgets(buff,size,f);
    int len = strlen(buff);
    while (len > 0 && buff[len-1] < 32 && buff[len-1] > 0)
	buff[--len] = 0;
    if (buff[0] == -17 && buff[1] == -69 && buff[2] == -65) { // 3 utf8 mark at beginning of file
	len -= 2;
	memmove(&buff[0],&buff[3],len);
    }
    return len;
}

void ReadTranslationFile(char *tr)
{
  xlate.Clear();
  translationLoaded = false;
  if (!tr || !tr[0]) return;
  if (tr != translation)
      strcpy(translation,tr);
  char path[FILENAME_MAX+17];
  char path2[FILENAME_MAX+17];
  snprintf(path,FILENAME_MAX+17,"%s/locale/orig.po",pwd);
  snprintf(path2,FILENAME_MAX+17,"%s/locale/%s",pwd,tr);
  FILE *f = fopen(path,"r");
  FILE *g = fopen(path2,"r");
  if (!f) {
      if (g) fclose(g);
      return;
  }
  if (!g) {
      if (f) fclose(f);
      return;
  }
  char buf[4096];
  char buf2[4096];
  int numLine = 0;
  while (myfgets(buf,4096,f))
  {
      if (!myfgets(buf2,4096,g)) {
	  UI_MessageBox("2nd translation file too short","Warning", MESSAGE_OK);
	  fclose(f);
	  fclose(g);
	  return;
      }
      numLine++;
      size_t len = strlen(buf);
      if (len == 0) continue;
      if ( buf[0] == '#') continue;
      xlate.AddTranslation(buf,buf2,numLine);
  };
  translationLoaded = true;
  fclose(f);
  fclose(g);
}

static char tr_linefeed[256];

const char *XTABL::Translate(const char *text)
{
  int idx, i;
  if (text[0] == 0) return text;
  int len = strlen(text);
  int space = (text[len-1] == ' ');
  if (text[0] == 10) {
      int pos = 1;
      tr_linefeed[0] = 10;
      while (text[pos] == 10)
	  tr_linefeed[pos++] = 10;
      strncpy(&tr_linefeed[pos],Translate(text+pos),256-pos);
      return tr_linefeed;
  }
  idx = text[0];
  if (text[0] != 0) idx += text[1];
  idx &= 0x1f;
  for (i=0; i<m_numXlate[idx]; i++)
  {
    if (strcmp(m_english[idx][i], text) == 0) return m_language[idx][i];
    if (space) {
	// Handle trailing space here because I like to use an editor which deletes trailing spaces at end of lines
	// It's actually very rarely used, the only occurence I know for now is "WEIGHS " when looking at an item on the eye.
	char buf[100];
	strncpy(buf,text,100);
	buf[len-1] = 0;
	if (strcmp(m_english[idx][i], buf) == 0) {
	    static char buf_space[1000];
	    snprintf(buf_space,1000,"%s ",m_language[idx][i]);
	    return buf_space;
	}
    }
  }
  if (translationLoaded) printf("not translated: %s.\n",text);
  return text;
}


const char *TranslateLanguage(const char *text)
{
  return xlate.Translate(text);
}

void TranslateWallLanguage(unsigned char *text)
{
  // character encoding -- A=0, B=1,... space=26;  period=27; end of line = 128; end of text=129
  int i;
  char *xText;
  for (i=0; text[i] != 129; i++)
  {
    if      (text[i] == 128) text[i] = '\n';
    else if (text[i] == 26)  text[i] = ' ';
    else if (text[i] == 27)  text[i] = '.';
    else                     text[i] = (char)('A' + text[i]);
  };
  text[i] = 0;
  xText = (char*)xlate.Translate((char *)text);
  if (!experimental_overlay) {
      // Reformat in 18 characters max the string, should have been done a very long time ago
      // will do differently with the experimental overlay
      int len=0,space=-1;
      for (size_t n=0; n<strlen(xText); n++) {
	  if (xText[n] == ' ') {
	      space = n;
	      len++;
	  } else if (xText[n] == 10)
	      len = 0;
	  else
	      len++;
	  if (len >= 18) {
	      if (space > -1) {
		  xText[space] = 10;
		  len = n-space;
		  space = -1;
	      }
	  }
      }
  }
  if (experimental_overlay) {
      if ((char*)text != xText)
	  strcpy((char*)text,xText);
      return;
  }

  for(i=0; xText[i] != 0; i++)
  {
    if ( (xText[i] >= 'A')  && (xText[i] <= 'Z') ) text[i] = (char)(xText[i] - 'A');
    else if (xText[i] == '\n') text[i] = 128;
    else if (xText[i] == '.')  text[i] = 27;
    else text[i] = 26;
  };
  text[i] = 129;
}

void CleanupTranslations(void)
{
  xlate.Clear();
}
