#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <string.h>

#define NONE -1234

int output = 0;   /* 0 = pdf, 1 = eps, 2=png */
int resolution = 90;


/* linked list of ids and labels of layers from the svg xml tree */
typedef struct _layer_t {
  xmlChar *id, *label;
  struct _layer_t *next;
} layer_t;

layer_t *layers=NULL;


/* range of overlays */
typedef struct {
  int lo,hi;
} seq_t;

void error(char * fmt, ... ) {
  va_list ap;

  printf("error: ");
  va_start(ap,fmt);
  vprintf(fmt, ap);
  va_end(ap);
  printf("\n");
  printf("for usage information try  -h \n");
  exit(1);
}


/* given a root node of a xml document,find first node whose property "name" has value "value" */
xmlNode * find_by_attr(xmlNode * a_node, const xmlChar * name, const xmlChar *value)
{
    xmlNode *cur_node = NULL;
    xmlChar *prop;
    xmlNode *result = NULL;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
           prop = xmlGetProp(cur_node,name);
           if (xmlStrEqual(prop,value)) {
             xmlFree(prop);
             return cur_node;
           }
           xmlFree(prop);
        }
        result =  find_by_attr(cur_node->children,name,value);
        if (result) break;
    }
    return result;
}

/* parse overlay specifications from layer's name. 
 * first call should supply label. 
 * it is copied to a static buffer and subsequent calls from the same label should use NULL
 * it returns a range of overlays, or [NONE,NONE] if there are no more ranges
 */
seq_t parse(xmlChar *label) {
  static char buf[10000];
  char *tok;
  seq_t result;
  int n;

  result.lo=result.hi=NONE;

  if (label) {
      if (label[0]!='[') return result;
      sprintf(buf,"%.9990s",label+1);
      *(strchrnul(buf,(int)']'))=(char)0;
      tok=strtok(buf,",");
  } else
      tok=strtok(NULL,",");
  if (!tok) return result;
  n=sscanf(tok,"%d-%d",&(result.lo),&(result.hi));
  if (n==1)
    result.hi=result.lo;
  return result;
}

/* parse the xml tree and create the list of layers */
layer_t *listLayers(xmlNode * a_node,  layer_t *list) {
    xmlNode *cur_node = NULL;
    xmlChar *groupmode;
    layer_t *result;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
           if (xmlStrEqual(cur_node->name,BAD_CAST "g")) {
             groupmode = xmlGetProp(cur_node,BAD_CAST "groupmode");
             if (xmlStrEqual(groupmode,BAD_CAST "layer")) {
                result = ( layer_t *) malloc(sizeof(layer_t));
                result->id    = xmlGetProp(cur_node,BAD_CAST "id");
                result->label = xmlGetProp(cur_node,BAD_CAST "label");
                result->next  = list;
                list          = result;
             }
             xmlFree(groupmode);
           }
        }
        list=listLayers(cur_node->children,list);
    }
    return list;
}

/* read the xml document from file */
xmlDocPtr getdoc (char *docname) {
  xmlDocPtr doc;

  doc = xmlParseFile(docname);
  if (doc == NULL ) {
    fprintf(stderr,"Document not parsed successfully. \n");
    return NULL;
  }
  return doc;
}


/* set the "style" property of given node to contain either "display:inline" or "display:none" */
void set_layer_visibility (xmlNode *layer,int visible) {
  xmlChar *style,*newstyle;
  char *s;

  style=xmlGetProp(layer,BAD_CAST "style");
  if (style) {
      newstyle=(char *)malloc(strlen(style)+30);
      newstyle[0]=0;
      s=strstr(style,"display:");
      if (s) {
        *s=0;
        sprintf(newstyle,"%s",style);
        for(s++;(*s)&&(*s!=';');s++);
        if (*s==';')
          sprintf(newstyle+strlen(newstyle),"%s;",s+1);
      }
  } else {
      newstyle=(char *)malloc(30);
      newstyle[0]=0;
  }
  sprintf(newstyle+strlen(newstyle),"display:%s",(visible)?"inline":"none");
  xmlSetProp(layer,BAD_CAST "style",BAD_CAST newstyle);

  xmlFree(style);
  free(newstyle);
}


/* print help and exit */
void print_usage(char *progname) {
  printf("usage: %s input.svg template [opitons]\n\n",progname);
  printf("  -e [ PDF | EPS | PNG] : export  (default is pdf)\n");
  printf("  -r <resolution> \n");
  printf("\nexample %s drawing.svg figure-##.pdf\n",progname);
  exit(1);
}


/* process commandline */
void parse_options(int argc, char **argv) {
    int i;

    for (i = 0 ; i < argc ; i++ ) {
        // -h
        if ( (!strcmp(argv[i],"-h")) || (!strcmp(argv[i],"-?")) || (!strcmp(argv[i],"--help")))
            print_usage(argv[0]);
        else if ( (!strcmp(argv[i],"-e"))) {
          i++;
          if (i>=argc)
            error("no output type given");
          if (!strcmp(argv[i],"EPS"))
            output = 1;
          else if (!strcmp(argv[i],"PDF"))
            output = 0;
          else if (!strcmp(argv[i],"PNG"))
            output = 2;
          else error("unknown output format");

        }
        else  if ( (!strcmp(argv[i],"-r"))) {
          i++;
          if (i>=argc)
            error("no resolution given");
          if (sscanf(argv[i],"%d",&resolution)!=1)
            error("wrong resolution");
        }
        else error("unknown option %s", argv[i]);
    }
}


int
main(int argc, char **argv)
{
    xmlDoc *doc = NULL;
    xmlNode *p = NULL;
    int i;
    layer_t *l;
    seq_t ls;
    int min=99999,max=-99999,run=0,vis;
    char cmd[1000],file[1000],tmpfile[10000],*templ,*s,*tmpname;
    int templen,tempstart;
    
    char **options,*optionbuf;
    int noptions;

    if (argc<3) print_usage(argv[0]);

    templ=strchr(argv[2],'#');
    if (!templ) {
      fprintf(stderr,"Template must contain #.\n");
      print_usage(argv[0]);
    }
    for (s=templ;*s=='#';s++);
    templen=s-templ;
    tempstart=templ-argv[2];

    LIBXML_TEST_VERSION

    doc = getdoc(argv[1]); /* read file */
    if (!doc) print_usage(argv[0]);

    layers=listLayers(xmlDocGetRootElement(doc),layers); /* get list of layers */
 
    /* find the maximum and minimum overlay */
    for (l=layers;l;l=l->next) {
      for (ls=parse(l->label);ls.lo!=NONE;ls=parse(NULL)){
        run=1;
        if (ls.lo<min) min=ls.lo;
        if (ls.hi>max) max=ls.hi;
      }
    }
    
    /* parse options from layer names */
    for (l=layers;l;l=l->next) if ((l->label)[0]=='-'){
      optionbuf = (char *)malloc(strlen(l->label)+10);
      strcpy(optionbuf,l->label);
      optionbuf[strlen(l->label)]=0;
      options= (char **)malloc(strlen(optionbuf)*sizeof(char*));

      for (s=optionbuf,noptions=0;1;) {
        if (*s==0) break;
        if (*s==' ') *(s++)=0;
        else {
          if ( (s==optionbuf) || (*(s-1)==0) )
            options[noptions++]=s;
          s++;
        }
      }

      parse_options(noptions,options);

      free(optionbuf);
      free(options);
    }

    
    parse_options(argc-3,argv+3);

    if (!run) {  /* nothing to do */
      fprintf(stderr,"No overlay specification found.\n");
      print_usage(argv[0]);
    }

    /* is template long enough? */
    sprintf(file,"%0d",max);
    if (strlen(file)>templen) {
      fprintf(stderr,"Overlay specification longer than template.\n");
      print_usage(argv[0]);
    }


    for (i=min;i<=max;i++) {
      printf("Overlay %d\n",i);
      for (l=layers;l;l=l->next) {
          printf("layer \"%s\" ",l->label);
          vis=0;
          for (ls=parse(l->label);ls.lo!=NONE;ls=parse(NULL)){
              if ( (ls.lo<=i) && (ls.hi>=i) ) {vis=1;break;}
          }
          printf("%svisible\n",vis?"":"in");
          p=find_by_attr(xmlDocGetRootElement(doc), BAD_CAST "id", l->id);
          set_layer_visibility(p,vis);
      }
      sprintf(file,"%s",argv[2]); /* prepare template */
      sprintf(file+tempstart,"%0*d",templen,i);
      sprintf(file+strlen(file),"%s",argv[2]+(tempstart+templen));

      tmpname=tempnam("/tmp",NULL);
      sprintf(tmpfile,"%s.svg",tmpname);
      free(tmpname);
      xmlSaveFile(tmpfile,doc);
      switch (output) {
        case 0:
          sprintf(cmd,"inkscape --export-pdf='%s' --export-area-drawing --without-gui %s",file,tmpfile);
          break;
        case 1:
          sprintf(cmd,"inkscape --export-eps='%s' --export-area-drawing --without-gui %s",file,tmpfile);
          break;
        case 2:
          sprintf(cmd,"inkscape --export-png='%s' --export-dpi %d --export-area-drawing --without-gui %s",file,resolution,tmpfile);
          break;
        default:
          error("bad output option (rhis really shouldn't happen)");
      }
      system(cmd);
      unlink(tmpfile);
    }

    
    //TODO: free layers    
    xmlFreeDoc(doc);
    xmlCleanupParser();

    return 0;
}  
  
  
  
  
