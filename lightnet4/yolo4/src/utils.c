#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <float.h>
#include <limits.h>
#include "image.h"

#include "utils.h"


int *read_intlist(char *gpu_list, int *ngpus, int d)
{
    int *gpus = 0;
    if(gpu_list){
        int len = strlen(gpu_list);
        *ngpus = 1;
        int i;
        for(i = 0; i < len; ++i){
            if (gpu_list[i] == ',') ++*ngpus;
        }
        gpus = calloc(*ngpus, sizeof(int));
        for(i = 0; i < *ngpus; ++i){
            gpus[i] = atoi(gpu_list);
            gpu_list = strchr(gpu_list, ',')+1;
        }
    } else {
        gpus = calloc(1, sizeof(float));
        *gpus = d;
        *ngpus = 1;
    }
    return gpus;
}

int *read_map(char *filename)
{
    int n = 0;
    int *map = 0;
    char *str;
    FILE *file = fopen(filename, "r");
    if(!file) file_error(filename);
    while((str=fgetl(file))){
        ++n;
        map = realloc(map, n*sizeof(int));
        map[n-1] = atoi(str);
    }
    return map;
}

void sorta_shuffle(void *arr, size_t n, size_t size, size_t sections)
{
    size_t i;
    for(i = 0; i < sections; ++i){
        size_t start = n*i/sections;
        size_t end = n*(i+1)/sections;
        size_t num = end-start;
        shuffle(arr+(start*size), num, size);
    }
}

void shuffle(void *arr, size_t n, size_t size)
{
    size_t i;
    void *swp = calloc(1, size);
    for(i = 0; i < n-1; ++i){
        size_t j = i + rand()/(RAND_MAX / (n-i)+1);
        memcpy(swp,          arr+(j*size), size);
        memcpy(arr+(j*size), arr+(i*size), size);
        memcpy(arr+(i*size), swp,          size);
    }
}

void del_arg(int argc, char **argv, int index)
{
    int i;
    for(i = index; i < argc-1; ++i) argv[i] = argv[i+1];
    argv[i] = 0;
}

int find_arg(int argc, char* argv[], char *arg)
{
    int i;
    for(i = 0; i < argc; ++i) {
        if(!argv[i]) continue;
        if(0==strcmp(argv[i], arg)) {
            del_arg(argc, argv, i);
            return 1;
        }
    }
    return 0;
}

int find_int_arg(int argc, char **argv, char *arg, int def)
{
    int i;
    for(i = 0; i < argc-1; ++i){
        if(!argv[i]) continue;
        if(0==strcmp(argv[i], arg)){
            def = atoi(argv[i+1]);
            del_arg(argc, argv, i);
            del_arg(argc, argv, i);
            break;
        }
    }
    return def;
}

float find_float_arg(int argc, char **argv, char *arg, float def)
{
    int i;
    for(i = 0; i < argc-1; ++i){
        if(!argv[i]) continue;
        if(0==strcmp(argv[i], arg)){
            def = atof(argv[i+1]);
            del_arg(argc, argv, i);
            del_arg(argc, argv, i);
            break;
        }
    }
    return def;
}

char *find_char_arg(int argc, char **argv, char *arg, char *def)
{
    int i;
    for(i = 0; i < argc-1; ++i){
        if(!argv[i]) continue;
        if(0==strcmp(argv[i], arg)){
            def = argv[i+1];
            del_arg(argc, argv, i);
            del_arg(argc, argv, i);
            break;
        }
    }
    return def;
}


char *basecfg(char *cfgfile)
{
    char *c = cfgfile;
    char *next;
    while((next = strchr(c, '/')))
    {
        c = next+1;
    }
    c = copy_string(c);
    next = strchr(c, '.');
    if (next) *next = 0;
    return c;
}

int alphanum_to_int(char c)
{
    return (c < 58) ? c - 48 : c-87;
}
char int_to_alphanum(int i)
{
    if (i == 36) return '.';
    return (i < 10) ? i + 48 : i + 87;
}

void pm(int M, int N, float *A)
{
    int i,j;
    for(i =0 ; i < M; ++i){
        printf("%d ", i+1);
        for(j = 0; j < N; ++j){
            printf("%2.4f, ", A[i*N+j]);
        }
        printf("\n");
    }
    printf("\n");
}

void find_replace(char *str, char *orig, char *rep, char *output)
{
    char buffer[4096] = {0};
    char *p;

    sprintf(buffer, "%s", str);
    if(!(p = strstr(buffer, orig))){  // Is 'orig' even in 'str'?
        sprintf(output, "%s", str);
        return;
    }

    *p = '\0';

    sprintf(output, "%s%s%s", buffer, rep, p+strlen(orig));
}

float sec(clock_t clocks)
{
    return (float)clocks/CLOCKS_PER_SEC;
}

void top_k(float *a, int n, int k, int *index)
{
    int i,j;
    for(j = 0; j < k; ++j) index[j] = -1;
    for(i = 0; i < n; ++i){
        int curr = i;
        for(j = 0; j < k; ++j){
            if((index[j] < 0) || a[curr] > a[index[j]]){
                int swap = curr;
                curr = index[j];
                index[j] = swap;
            }
        }
    }
}

void error(const char *s)
{
    perror(s);
    assert(0);
    exit(-1);
}

void malloc_error()
{
    fprintf(stderr, "Malloc error\n");
    exit(-1);
}

void file_error(char *s)
{
    fprintf(stderr, "Couldn't open file: %s\n", s);
    exit(0);
}

list *split_str(char *s, char delim)
{
    size_t i;
    size_t len = strlen(s);
    list *l = make_list();
    list_insert(l, s);
    for(i = 0; i < len; ++i){
        if(s[i] == delim){
            s[i] = '\0';
            list_insert(l, &(s[i+1]));
        }
    }
    return l;
}

void strip(char *s)
{
    size_t i;
    size_t len = strlen(s);
    size_t offset = 0;
    for(i = 0; i < len; ++i){
        char c = s[i];
        if(c==' '||c=='\t'||c=='\n') ++offset;
        else s[i-offset] = c;
    }
    s[len-offset] = '\0';
}

void strip_char(char *s, char bad)
{
    size_t i;
    size_t len = strlen(s);
    size_t offset = 0;
    for(i = 0; i < len; ++i){
        char c = s[i];
        if(c==bad) ++offset;
        else s[i-offset] = c;
    }
    s[len-offset] = '\0';
}

void free_ptrs(void **ptrs, int n)
{
    int i;
    for(i = 0; i < n; ++i) free(ptrs[i]);
    free(ptrs);
}

char *fgetl(FILE *fp)
{
    if(feof(fp)) return 0;
    size_t size = 512;
    char *line = malloc(size*sizeof(char));
    if(!fgets(line, size, fp)){
        free(line);
        return 0;
    }

    size_t curr = strlen(line);

    while((line[curr-1] != '\n') && !feof(fp)){
        if(curr == size-1){
            size *= 2;
            line = realloc(line, size*sizeof(char));
            if(!line) {
                printf("%ld\n", size);
                malloc_error();
            }
        }
        size_t readsize = size-curr;
        if(readsize > INT_MAX) readsize = INT_MAX-1;
        fgets(&line[curr], readsize, fp);
        curr = strlen(line);
    }
    if(line[curr-1] == '\n') line[curr-1] = '\0';

    return line;
}

int read_int(int fd)
{
    int n = 0;
    int next = read(fd, &n, sizeof(int));
    if(next <= 0) return -1;
    return n;
}

void write_int(int fd, int n)
{
    int next = write(fd, &n, sizeof(int));
    if(next <= 0) error("read failed");
}

int read_all_fail(int fd, char *buffer, size_t bytes)
{
    size_t n = 0;
    while(n < bytes){
        int next = read(fd, buffer + n, bytes-n);
        if(next <= 0) return 1;
        n += next;
    }
    return 0;
}

int write_all_fail(int fd, char *buffer, size_t bytes)
{
    size_t n = 0;
    while(n < bytes){
        size_t next = write(fd, buffer + n, bytes-n);
        if(next <= 0) return 1;
        n += next;
    }
    return 0;
}

void read_all(int fd, char *buffer, size_t bytes)
{
    size_t n = 0;
    while(n < bytes){
        int next = read(fd, buffer + n, bytes-n);
        if(next <= 0) error("read failed");
        n += next;
    }
}

void write_all(int fd, char *buffer, size_t bytes)
{
    size_t n = 0;
    while(n < bytes){
        size_t next = write(fd, buffer + n, bytes-n);
        if(next <= 0) error("write failed");
        n += next;
    }
}


char *copy_string(char *s)
{
    char *copy = malloc(strlen(s)+1);
    strncpy(copy, s, strlen(s)+1);
    return copy;
}

list *parse_csv_line(char *line)
{
    list *l = make_list();
    char *c, *p;
    int in = 0;
    for(c = line, p = line; *c != '\0'; ++c){
        if(*c == '"') in = !in;
        else if(*c == ',' && !in){
            *c = '\0';
            list_insert(l, copy_string(p));
            p = c+1;
        }
    }
    list_insert(l, copy_string(p));
    return l;
}

int count_fields(char *line)
{
    int count = 0;
    int done = 0;
    char *c;
    for(c = line; !done; ++c){
        done = (*c == '\0');
        if(*c == ',' || done) ++count;
    }
    return count;
}

float *parse_fields(char *line, int n)
{
    float *field = calloc(n, sizeof(float));
    char *c, *p, *end;
    int count = 0;
    int done = 0;
    for(c = line, p = line; !done; ++c){
        done = (*c == '\0');
        if(*c == ',' || done){
            *c = '\0';
            field[count] = strtod(p, &end);
            if(p == c) field[count] = nan("");
            if(end != c && (end != c-1 || *end != '\r')) field[count] = nan(""); //DOS file formats!
            p = c+1;
            ++count;
        }
    }
    return field;
}

float sum_array(float *a, int n)
{
    int i;
    float sum = 0;
    for(i = 0; i < n; ++i) sum += a[i];
    return sum;
}

float mean_array(float *a, int n)
{
    return sum_array(a,n)/n;
}

void mean_arrays(float **a, int n, int els, float *avg)
{
    int i;
    int j;
    memset(avg, 0, els*sizeof(float));
    for(j = 0; j < n; ++j){
        for(i = 0; i < els; ++i){
            avg[i] += a[j][i];
        }
    }
    for(i = 0; i < els; ++i){
        avg[i] /= n;
    }
}

void print_statistics(float *a, int n)
{
    float m = mean_array(a, n);
    float v = variance_array(a, n);
    printf("MSE: %.6f, Mean: %.6f, Variance: %.6f\n", mse_array(a, n), m, v);
}

float variance_array(float *a, int n)
{
    int i;
    float sum = 0;
    float mean = mean_array(a, n);
    for(i = 0; i < n; ++i) sum += (a[i] - mean)*(a[i]-mean);
    float variance = sum/n;
    return variance;
}

int constrain_int(int a, int min, int max)
{
    if (a < min) return min;
    if (a > max) return max;
    return a;
}

float constrain(float min, float max, float a)
{
    if (a < min) return min;
    if (a > max) return max;
    return a;
}

float dist_array(float *a, float *b, int n, int sub)
{
    int i;
    float sum = 0;
    for(i = 0; i < n; i += sub) sum += pow(a[i]-b[i], 2);
    return sqrt(sum);
}

float mse_array(float *a, int n)
{
    int i;
    float sum = 0;
    for(i = 0; i < n; ++i) sum += a[i]*a[i];
    return sqrt(sum/n);
}

void normalize_array(float *a, int n)
{
    int i;
    float mu = mean_array(a,n);
    float sigma = sqrt(variance_array(a,n));
    for(i = 0; i < n; ++i){
        a[i] = (a[i] - mu)/sigma;
    }
    mu = mean_array(a,n);
    sigma = sqrt(variance_array(a,n));
}

void translate_array(float *a, int n, float s)
{
    int i;
    for(i = 0; i < n; ++i){
        a[i] += s;
    }
}

float mag_array(float *a, int n)
{
    int i;
    float sum = 0;
    for(i = 0; i < n; ++i){
        sum += a[i]*a[i];   
    }
    return sqrt(sum);
}

void scale_array(float *a, int n, float s)
{
    int i;
    for(i = 0; i < n; ++i){
        a[i] *= s;
    }
}

int sample_array(float *a, int n)
{
    float sum = sum_array(a, n);
    scale_array(a, n, 1./sum);
    float r = rand_uniform(0, 1);
    int i;
    for(i = 0; i < n; ++i){
        r = r - a[i];
        if (r <= 0) return i;
    }
    return n-1;
}

int max_index(float *a, int n)
{
    if(n <= 0) return -1;
    int i, max_i = 0;
    float max = a[0];
    for(i = 1; i < n; ++i){
        if(a[i] > max){
            max = a[i];
            max_i = i;
        }
    }
    return max_i;
}

int rand_int(int min, int max)
{
    if (max < min){
        int s = min;
        min = max;
        max = s;
    }
    int r = (rand()%(max - min + 1)) + min;
    return r;
}

// From http://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform
float rand_normal()
{
    static int haveSpare = 0;
    static double rand1, rand2;

    if(haveSpare)
    {
        haveSpare = 0;
        return sqrt(rand1) * sin(rand2);
    }

    haveSpare = 1;

    rand1 = rand() / ((double) RAND_MAX);
    if(rand1 < 1e-100) rand1 = 1e-100;
    rand1 = -2 * log(rand1);
    rand2 = (rand() / ((double) RAND_MAX)) * TWO_PI;

    return sqrt(rand1) * cos(rand2);
}

/*
   float rand_normal()
   {
   int n = 12;
   int i;
   float sum= 0;
   for(i = 0; i < n; ++i) sum += (float)rand()/RAND_MAX;
   return sum-n/2.;
   }
 */

size_t rand_size_t()
{
    return  ((size_t)(rand()&0xff) << 56) | 
            ((size_t)(rand()&0xff) << 48) |
            ((size_t)(rand()&0xff) << 40) |
            ((size_t)(rand()&0xff) << 32) |
            ((size_t)(rand()&0xff) << 24) |
            ((size_t)(rand()&0xff) << 16) |
            ((size_t)(rand()&0xff) << 8) |
            ((size_t)(rand()&0xff) << 0);
}

float rand_uniform(float min, float max)
{
    if(max < min){
        float swap = min;
        min = max;
        max = swap;
    }
    return ((float)rand()/RAND_MAX * (max - min)) + min;
}

float rand_scale(float s)
{
    float scale = rand_uniform(1, s);
    if(rand()%2) return scale;
    return 1./scale;
}

float **one_hot_encode(float *a, int n, int k)
{
    int i;
    float **t = calloc(n, sizeof(float*));
    for(i = 0; i < n; ++i){
        t[i] = calloc(k, sizeof(float));
        int index = (int)a[i];
        t[i][index] = 1;
    }
    return t;
}

int extractIndexFromFloat(double degreeStoreElement){

    double integral;
    double fractional;

    fractional = modf(degreeStoreElement, &integral);

    //printf("integral part: %0.0f\n", integral);
    //printf("fractional part: %0.2f\n", fractional);

	fractional = fractional*100.0f;
	fractional = (fractional > (floor(fractional)+0.5f)) ? ceil(fractional) : floor(fractional);
	int out=(int)fractional;
	return out;
}

int searchWithDirection(float** probs, int num, int currentBase, int classIndex, double degree){

	double cell=degree/90;
	int lower=floor(cell);
	int upper=ceil(cell);
	int sum=lower*10+upper;
	int totalcell=num/5;
	int totalrow=(int)sqrt(totalcell);
	//float maxCellandProb=0;
	int maxCellIndex=0;


	switch(sum){

		case 0:
			//0,0
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase+1, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase+1-totalrow, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase+1+totalrow, classIndex, maxCellIndex);
			return maxCellIndex;

		case 1:
			//0,1
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase+1, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase+1-totalrow, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase-totalrow, classIndex, maxCellIndex);
			return maxCellIndex;
//			currentBase+1
//			currentBase+1-totalrow
//			currentBase-totalrow
		case 11:
			//1,1
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase-totalrow, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase+1-totalrow, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell,currentBase-1-totalrow, classIndex, maxCellIndex);
			return maxCellIndex;

//			currentBase-totalrow
//			currentBase+1-totalrow
//			currentBase-1-totalrow
		case 12:
			//1,2
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase-totalrow, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase-1-totalrow, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase-1, classIndex, maxCellIndex);
			return maxCellIndex;
//			currentBase-totalrow
//			currentBase-1-totalrow
//			currentBase-1
		case 22:
			//2,2
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase-1, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase-1-totalrow, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase-1+totalrow, classIndex, maxCellIndex);
			return maxCellIndex;
//			currentBase-1
//			currentBase-1-totalrow
//			currentBase-1+totalrow
		case 23:
			//2,3
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase-1, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase-1+totalrow, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase+totalrow, classIndex, maxCellIndex);
			return maxCellIndex;
//			currentBase-1
//			currentBase-1+totalrow
//			currentBase+totalrow
		case 33:
			//3,3
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase+totalrow, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase-1+totalrow, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase+1+totalrow, classIndex, maxCellIndex);
			return maxCellIndex;
//			currentBase+totalrow
//			currentBase-1+totalrow
//			currentBase+1+totalrow
		case 34:
			//3,4=3.0
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase+totalrow, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase+1+totalrow, classIndex, maxCellIndex);
			maxCellIndex=lookIntoCells(probs, totalcell, currentBase+1, classIndex, maxCellIndex);
			return maxCellIndex;
//			currentBase+totalrow
//			currentBase+1+totalrow
//			currentBase+1

	}

	printf("Nothing gets updated!\n");
	return maxCellIndex;

}



int lookIntoCells(float** probs, int totalcell, int cellIndex, int classIndex, int maxCellIndex){
	int nn_level;
	int possibleIndex;
	float possibleProb;

	float maxProb=probs[maxCellIndex][classIndex];

	for(nn_level=0;nn_level<5;nn_level++){
		possibleIndex=cellIndex+totalcell*nn_level;
		possibleProb=probs[possibleIndex][classIndex];
		if (possibleProb>maxProb){
			maxProb=possibleProb;
			maxCellIndex=possibleIndex;
		}


	}
	if (maxProb>1.0){
		assert(0);
		printf("maxProb: %0.2f, which is larger than 1!!!\n", maxProb);
	}

	return maxCellIndex;
}

int searchWithoutDirection(float** probs, int num, int currentBase, int classIndex, int bump){


	int totalcell=num/5;
	int totalrow=(int)sqrt(totalcell);
	int maxCellIndex=0;
	float maxProb=probs[maxCellIndex][classIndex];


	int nn_level;
	for(nn_level=0;nn_level<5;nn_level++){
		int row;
		for (row=-1;row<2;row++){
			int i;
			for (i=-1;i<2;i++){
				int possibleIndex=(currentBase+i+totalrow*row)+totalcell*nn_level;
				float possibleProb=probs[possibleIndex][classIndex];

				if(possibleProb>maxProb){
					maxCellIndex=possibleProb;
					maxProb=possibleProb;
				}


			}
		}
	}
	return maxCellIndex;

}

int addDegree(int degree1, int degree2){
	float pi=3.1415926;
	if(degree1>180){
		degree1=-(360-degree1);

	}
	if(degree2>180){
		degree2=-(360-degree2);
	}


	float x1=1*cos(degree1*(pi/180));
	float y1=1*sin(degree1*(pi/180));

	float x2=1*cos(degree2*(pi/180));
	float y2=1*sin(degree2*(pi/180));

	float sumx=(x1+x2)/2;
	float sumy=(y1+y2)/2;


	if(fabs(sumx)<0.00000001 || fabs(sumy)<0.00000001){
		return 0;
	}
	int out;


	if (sumx>0 &&sumy>0){
		out=atan(fabs(sumy)/fabs(sumx))*(180/3.1415926);
	}

	else if (sumx<0 && sumy>0){
		out=atan(fabs(sumy)/fabs(sumx))*(180/3.1415926);
		out=180-out;
	}
	else if (sumx<0 && sumy<0){
		out=atan(fabs(sumy)/fabs(sumx))*(180/3.1415926);
		out=180+out;

	}

	else if (sumx>0 && sumy<0){
		out=atan(fabs(sumy)/fabs(sumx))*(180/3.1415926);
		out=360-out;

	}
	else {
		printf("Error!\n");
		out=100000;
	}

	return out;

}

int addMagnitude(int m1, int m2){
	int out=(m1+m2)/2;
	return out;

}

Opticalflow mergeMedian(double* degreeStore, int end, int range){

    int medianOfMedian=end*0.75;

    int s;
    int sScope=3;
    int mergedDegree=0;
    int mergedMagnitude=0;
    for(s=(-sScope/2);s<(sScope/2+1);s++){

    	double degreeStoreElement=degreeStore[medianOfMedian+s];
    	int medianMagnitude=extractIndexFromFloat(degreeStoreElement);
    	int medianDegree=(int)degreeStoreElement;
    	printf("medianDegree: %i, medianMagnitude: %i\n", medianDegree, medianMagnitude);
    	if(s==(-sScope/2)){
    		//if not, the first degree will be affected by degree of 0
    		mergedDegree=medianDegree;
    		mergedMagnitude=medianMagnitude;
    	}

    	mergedDegree=addDegree(mergedDegree, medianDegree);
    	mergedMagnitude=addMagnitude(mergedMagnitude, medianMagnitude);

    }

    printf("mergedDegree: %i, mergdeMagnitude: %i\n", mergedDegree, mergedMagnitude);
    Opticalflow medianflow=create_opticalflowFB(mergedDegree, mergedMagnitude);
    return medianflow;

}



