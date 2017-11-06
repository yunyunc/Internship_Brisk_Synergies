
#include "image.h"
#include "utils.h"
#include "blas.h"
#include "cuda.h"
#include "linkedlist.h"
#include "linkedlist.c"
#include "sortAlgorithms.h"
#include "sortAlgorithms.c"


#include "kalmanbox.h"
#include "kalmanbox.c"

#include "objectbank.h"
#include "objectbank.c"

#include "hashtable.c"

#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc_c.h"

#include <stdio.h>
#include <math.h>
#include <tgmath.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "opencv2/legacy/compat.hpp"
#include "opencv2/core/mat.hpp"



static int frame_num=0;  //ADDED: count for the frame number
static image pre_im;	 //ADDED: store the previous image
static int object_num=0; //ADDED: count for the number of objects in previous frame
static int object_prenum=0;
static int *idx_tempprestore;
static int saveDetection=1;
static int saveOpticalflow=1;
static int objectIndex=0;
static Boxflow *box_tempfull;
static Boxflow *box_Adfull;	//ADDED: additional storage place to store those previous objects for a certain frame duration
static snode* headconstant;
Opticalflow average_Ad; //ADDED: the optical flow vector computed about box_Adfull[i]

//TODO: Check static issue
static kalmanbox* temp_kalmanbox;
DataItem* hashArray[SIZE];

int windows = 0;


float colors[6][3] = { {1,0,1}, {0,0,1},{0,1,1},{0,1,0},{1,1,0},{1,0,0} };

float get_color(int c, int x, int max)
{
    float ratio = ((float)x/max)*5;
    int i = floor(ratio);
    int j = ceil(ratio);
    ratio -= i;
    float r = (1-ratio) * colors[i][c] + ratio*colors[j][c];
    //printf("%f\n", r);
    return r;
}

void composite_image(image source, image dest, int dx, int dy)
{
    int x,y,k;
    for(k = 0; k < source.c; ++k){
        for(y = 0; y < source.h; ++y){
            for(x = 0; x < source.w; ++x){
                float val = get_pixel(source, x, y, k);
                float val2 = get_pixel_extend(dest, dx+x, dy+y, k);
                set_pixel(dest, dx+x, dy+y, k, val * val2);
            }
        }
    }
}

image border_image(image a, int border)
{
    image b = make_image(a.w + 2*border, a.h + 2*border, a.c);
    int x,y,k;
    for(k = 0; k < b.c; ++k){
        for(y = 0; y < b.h; ++y){
            for(x = 0; x < b.w; ++x){
                float val = get_pixel_extend(a, x - border, y - border, k);
                if(x - border < 0 || x - border >= a.w || y - border < 0 || y - border >= a.h) val = 1;
                set_pixel(b, x, y, k, val);
            }
        }
    }
    return b;
}

image tile_images(image a, image b, int dx)
{
    if(a.w == 0) return copy_image(b);
    image c = make_image(a.w + b.w + dx, (a.h > b.h) ? a.h : b.h, (a.c > b.c) ? a.c : b.c);
    fill_cpu(c.w*c.h*c.c, 1, c.data, 1);
    embed_image(a, c, 0, 0); 
    composite_image(b, c, a.w + dx, 0);
    return c;
}

image get_label(image **characters, char *string, int size)
{
    if(size > 7) size = 7;
    image label = make_empty_image(0,0,0);
    while(*string){
        image l = characters[size][(int)*string];
        image n = tile_images(label, l, -size - 1 + (size+1)/2);
        free_image(label);
        label = n;
        ++string;
    }
    image b = border_image(label, label.h*.25);
    free_image(label);
    return b;
}

void draw_label(image a, int r, int c, image label, const float *rgb)
{
    int w = label.w;
    int h = label.h;
    if (r - h >= 0) r = r - h;

    int i, j, k;
    for(j = 0; j < h && j + r < a.h; ++j){
        for(i = 0; i < w && i + c < a.w; ++i){
            for(k = 0; k < label.c; ++k){
                float val = get_pixel(label, i, j, k);
                set_pixel(a, i+c, j+r, k, rgb[k] * val);
            }
        }
    }
}

void draw_box(image a, int x1, int y1, int x2, int y2, float r, float g, float b)
{
    //normalize_image(a);
    int i;
    if(x1 < 0) x1 = 0;
    if(x1 >= a.w) x1 = a.w-1;
    if(x2 < 0) x2 = 0;
    if(x2 >= a.w) x2 = a.w-1;

    if(y1 < 0) y1 = 0;
    if(y1 >= a.h) y1 = a.h-1;
    if(y2 < 0) y2 = 0;
    if(y2 >= a.h) y2 = a.h-1;

    for(i = x1; i <= x2; ++i){
        a.data[i + y1*a.w + 0*a.w*a.h] = r;
        a.data[i + y2*a.w + 0*a.w*a.h] = r;

        a.data[i + y1*a.w + 1*a.w*a.h] = g;
        a.data[i + y2*a.w + 1*a.w*a.h] = g;

        a.data[i + y1*a.w + 2*a.w*a.h] = b;
        a.data[i + y2*a.w + 2*a.w*a.h] = b;
    }
    for(i = y1; i <= y2; ++i){
        a.data[x1 + i*a.w + 0*a.w*a.h] = r;
        a.data[x2 + i*a.w + 0*a.w*a.h] = r;

        a.data[x1 + i*a.w + 1*a.w*a.h] = g;
        a.data[x2 + i*a.w + 1*a.w*a.h] = g;

        a.data[x1 + i*a.w + 2*a.w*a.h] = b;
        a.data[x2 + i*a.w + 2*a.w*a.h] = b;
    }
}

void draw_box_width(image a, int x1, int y1, int x2, int y2, int w, float r, float g, float b)
{
    int i;
    for(i = 0; i < w; ++i){
        draw_box(a, x1+i, y1+i, x2-i, y2-i, r, g, b);
    }
}

void draw_bbox(image a, box bbox, int w, float r, float g, float b)
{
    int left  = (bbox.x-bbox.w/2)*a.w;
    int right = (bbox.x+bbox.w/2)*a.w;
    int top   = (bbox.y-bbox.h/2)*a.h;
    int bot   = (bbox.y+bbox.h/2)*a.h;

    int i;
    for(i = 0; i < w; ++i){
        draw_box(a, left+i, top+i, right-i, bot-i, r, g, b);
    }
}

image **load_alphabet()
{
    int i, j;
    const int nsize = 8;
    image **alphabets = calloc(nsize, sizeof(image));
    for(j = 0; j < nsize; ++j){
        alphabets[j] = calloc(128, sizeof(image));
        for(i = 32; i < 127; ++i){
            char buff[256];
            sprintf(buff, "data/labels/%d_%d.png", i, j);
            alphabets[j][i] = load_image_color(buff, 0, 0);
        }
    }
    return alphabets;
}

int getframe_num(void *ptr)
{
	return frame_num;
}

void initialize_idx_prestore(int size, int totalcell2){
	idx_tempprestore=(int *)calloc(size, sizeof(int));
    box_tempfull=(Boxflow *)calloc(totalcell2, sizeof(Boxflow));
    box_Adfull=(Boxflow *)calloc(3, sizeof(Boxflow));
	return;
}

int compareFlowVector(int preFlow, int nowFlow, int preMag, int nowMag, double tolerance){
	int thMag=tolerance;
	int thFlow=30*tolerance;

	if(abs(preMag-nowMag)<thMag){
		return 1;
	}
	else if (abs(preFlow-nowFlow)<thFlow){
		return 1;
	}
	else{
		int maxnumber;
		int minnumber;
		if(preFlow>nowFlow){
			maxnumber=preFlow;
			minnumber=nowFlow;
		}
		else{
			maxnumber=nowFlow;
			minnumber=preFlow;

		}

		if(minnumber+360-maxnumber<thFlow){
			return 1;
		}

	}
	return 0;
}

int objectMatch(int num, int preFlow, int nowFlow, int preMag, int nowMag, int preClass, int nowClass, int preIndex, int nowIndex){
	int totalcell=num/5;
	int totalrow=(int)sqrt(totalcell);
	if(preClass!=nowClass){
		return 0;
	}

	else if(preIndex%totalcell==nowIndex%totalcell){
		return compareFlowVector(preFlow, nowFlow, preMag,nowMag,6);
	}

	else if(compareFlowVector(preFlow, nowFlow, preMag,nowMag,1)){
		int base=preIndex%totalcell;
		int nn_level;
		for(nn_level=0;nn_level<5;nn_level++){
			int row;
			for (row=-1;row<2;row++){
				int i;
				for (i=-1;i<2;i++){
					int possibleIndex=(base+i+totalrow*row)+totalcell*nn_level;

					if(possibleIndex==nowIndex){
						return 1;
					}


				}
			}
		}

	}

	return 0;

}

int lookAround(float** probsLastFrame, float** probs, int num, int prevIndex, int classIndex, float prevProb, float thresh, double prevDegree){
	//prevIndex: location index from the previous frame
	//prevProb: Prob of being certain class in last frame
	prevProb=prevProb/100.0;
	int totalcell=num/5;
	int currentBase=prevIndex%totalcell;
	float bump=0;
	int maxCellIndex;

	float currentProb=probs[prevIndex][classIndex];
	float percentDiff=(prevProb-currentProb)/prevProb;
	printf("\t probs[%i] of class[%i] drops by %0.2f\n", prevIndex, classIndex, percentDiff);


//	int iii;
//	int bbb=200;
//	for(iii=0;iii<1;iii++){
//		probs[iii+bbb+676*0][3]=1;
//		probs[iii+bbb+676*1][0]=1;
//		probs[iii+bbb+676*2][0]=1;
//		probs[iii+bbb+676*3][0]=1;
//		probs[iii+bbb+676*4][0]=1;
//	}

//	probs[173+676*0][0]=1;
//	probs[174+676*0][0]=1;
//	probs[199+676*0][3]=1;
//	probs[200+676*0][0]=1;
//
//	probs[173+676*1][0]=1;
//	probs[174+676*1][0]=1;
//	probs[199+676*1][0]=1;
//	probs[200+676*1][0]=1;


	if(currentProb<thresh){
		maxCellIndex=searchWithDirection(probs, num, currentBase, classIndex, prevDegree);
		float maxProb=probs[maxCellIndex][classIndex];
		printf("\t maxCellIndex: %i of %0.3f\n", maxCellIndex, maxProb);

		float preAdjProb=probsLastFrame[maxCellIndex][classIndex];
		float percentAdjDiff=(maxProb-preAdjProb)/preAdjProb;
		printf("\t probs[%i] of class[%i] increases by %0.2f\n", maxCellIndex, classIndex, percentAdjDiff);


		probs[maxCellIndex][classIndex]=probs[maxCellIndex][classIndex]+bump;
		probs[maxCellIndex][80]=probs[maxCellIndex][classIndex];

	}


 	return 0;

}

void draw_tracking(IplImage *im_frame, int left, int top, int width, int height, int color1, int color2, int color3, int objectIndex){
	CvPoint lefttop=cvPoint(left, top);
	CvPoint rightbot=cvPoint(left+width, top+height);
	CvScalar color=CV_RGB(color1, color2, color3);
	cvRectangle(im_frame, lefttop, rightbot, color, 3, 8, 0 );
    char object[sizeof(objectIndex)];
    sprintf(object, "%d", objectIndex);
	CvFont font;
	cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 2, 2, 0, 3, 8);
	cvPutText(im_frame, object, cvPoint(left, top), &font, color);

    cvNamedWindow("im_frame",CV_WINDOW_NORMAL);
    cvShowImage("im_frame",im_frame);

	return;
}

void saveUnmatched(IplImage *im_frame, Boxflow in){

		printf("\t Hey, there!\n");

		//TODO: Need the info from optical flow and kalman filter to set a speed offset
		//DataItem* temp_DataItem=hashsearch(hashArray, box_full[headnumber].objectIndex);
//		double xoffset=in.flow.magnitude*cos(in.flow.degree/180*3.1415926);
//		int xoff=(fabsf(xoffset-(int)xoffset)>0.5)?(xoffset+1):xoffset;
//		double yoffset=in.flow.magnitude*sin(in.flow.degree/180*3.1415926);
//		int yoff=-(fabsf(yoffset-(int)yoffset)>0.5)?(yoffset+1):yoffset;

		Boxflow temp=in;
		int left=temp.left+4;
		int top=temp.top-5;
		int width=temp.width;
		int height=temp.height;

		temp.left=left;
		temp.top=top;
		temp.width=width;
		temp.height=height;

		box_Adfull[0]=temp;

    	draw_tracking(im_frame, left, top, width, height, 0, 0, 52, 5);
        cvWaitKey(0);
        return;
}

int calculateOverlapping(Boxflow unmatched, int* current){
	int xleft=current[0];
	int xtop=current[1];
	int xright=current[0]+current[2];
	int xbottom=current[1]+current[3];

	int aleft=unmatched.left;
	int atop=unmatched.top;
	int aright=unmatched.width+unmatched.left;
	int abottom=unmatched.height+unmatched.top;

	if(aright<xleft || aleft>xright || abottom<xtop || atop>xbottom){
		return 0;
	}

	return 1;
}


void draw_detections(image im, int num, float thresh, box *boxes, float **probs, char **names, image **alphabet, int classes, int **box_para, int *idx_store, Boxflow *box_full)
{
    int i;
    int idx_count=0;
    int debug_frame=12;
    //int debug_object_index=3;
    image screenshot=copy_image(im);


    if(object_num>0){//frame_num=2 and above
    	int p;
        IplImage *im_frame=cvCreateImage(cvSize(im.w,im.h), IPL_DEPTH_8U, im.c);
        im_frame=image_convert_IplImage(im, im_frame);

        if(frame_num>debug_frame){
            printf("Wake up!\n");
        }


        if(box_Adfull[0].objectIndex==5){
            IplImage *boxcrop2=cvCreateImage(cvSize(im.w,im.h), IPL_DEPTH_8U, im.c);
            boxcrop2=image_convert_IplImage(im, boxcrop2);

            IplImage *pre_boxcrop2=cvCreateImage(cvSize(pre_im.w,pre_im.h), IPL_DEPTH_8U, pre_im.c);
            pre_boxcrop2=image_convert_IplImage(pre_im, pre_boxcrop2);

            cvSetImageROI(pre_boxcrop2, cvRect(box_Adfull[0].left, box_Adfull[0].top, box_Adfull[0].width, box_Adfull[0].height));
            cvSetImageROI(boxcrop2, cvRect(box_Adfull[0].left, box_Adfull[0].top, box_Adfull[0].width, box_Adfull[0].height));
            average_Ad=compute_opticalflowFB(pre_boxcrop2, boxcrop2);
            box_Adfull[0].flow=average_Ad;
        }


    	for(p=0;p<object_num;p++){

    		//TODO: Need to optimize to speed up later
            IplImage *boxcrop=cvCreateImage(cvSize(im.w,im.h), IPL_DEPTH_8U, im.c);
            boxcrop=image_convert_IplImage(im, boxcrop);

            IplImage *pre_boxcrop=cvCreateImage(cvSize(pre_im.w,pre_im.h), IPL_DEPTH_8U, pre_im.c);
            pre_boxcrop=image_convert_IplImage(pre_im, pre_boxcrop);

            cvSetImageROI(pre_boxcrop, cvRect(box_para[idx_store[p]][0], box_para[idx_store[p]][1], box_para[idx_store[p]][2], box_para[idx_store[p]][3]));
            cvSetImageROI(boxcrop, cvRect(box_para[idx_store[p]][0], box_para[idx_store[p]][1], box_para[idx_store[p]][2], box_para[idx_store[p]][3]));

            printf("\n");
            printf("1. Calculate Optical Flow: \n");
        	//Opticalflow average_result=compute_opticalflow(pre_boxcrop, boxcrop, box_para[idx_store[p]][0], box_para[idx_store[p]][1]);
        	Opticalflow average_result=compute_opticalflowFB(pre_boxcrop, boxcrop);


        	int match=0; //current matches previous objects from box_full
        	int match2=0; //current rematches saved objects from box_Adfull
        	int overlap=0;
        	printf("2. Match and Update: \n");
    		printf("\t Current: idx_store[p]: %i degree: %0.0f mag: %0.0f\n", idx_store[p], average_result.degree, average_result.magnitude);
    		snode* headcount=headconstant;

        	while (headcount!=NULL){//frame>2
        			int headnumber=headcount->data;
            		printf("\t Pre: idx_prestore[p]: %i degree: %0.0f mag: %0.0f objectIndex: %i\n", headnumber, box_full[headnumber].flow.degree, box_full[headnumber].flow.magnitude, box_full[headnumber].objectIndex);

            		int preFlow=box_full[headnumber].flow.degree;
            		int preMag=box_full[headnumber].flow.magnitude;

            		match=objectMatch(num, preFlow, average_result.degree, preMag, average_result.magnitude, box_full[headnumber].classtype, box_para[idx_store[p]][4], headnumber, idx_store[p]);
                	if(match==1){
                		box_para[idx_store[p]][9]=box_full[headnumber].objectIndex;

                		Boxflow nullflow=putNullInsideBox();
                		printf("\t %i matches with %i, with objectIndex: %i\n", idx_store[p], headnumber, box_full[headnumber].objectIndex);

//                		if(box_full[headnumber].objectIndex==14){
//                			probs[headnumber][0]=1;
//                			probs[headnumber+676][0]=1;
//                			probs[headnumber+676*2][0]=1;
//                			probs[headnumber+676*3][0]=1;
//                			probs[headnumber+676*4][0]=1;
//
//                		}

                		box_full[headnumber]=nullflow;
                		headconstant=remove_any(headconstant,headcount);
                		average_result=updateFlow(average_result, preFlow, 0.5);
                		printf("\t Degree updates to %0.0f\n", average_result.degree);
                		object_prenum=object_prenum-1;

                		break;

                	}

                	//TODO: try to match the optical flow inside box_Adfull
                	overlap=calculateOverlapping(box_Adfull[0], box_para[idx_store[p]]);
            		match2=objectMatch(num, average_Ad.degree, average_result.degree, average_Ad.magnitude, average_result.magnitude, box_Adfull[0].classtype, box_para[idx_store[p]][4], idx_store[p], idx_store[p]);

                	if(overlap && match2){

                		printf("\t %i REMATCHES with box_Adfull[%i], with objectIndex: %i\n", idx_store[p], 0, box_Adfull[0].objectIndex);
                    	box_para[idx_store[p]][9]=box_Adfull[0].objectIndex;
                    	Boxflow nullflow=putNullInsideBox();
                    	box_Adfull[0]=nullflow;
                    	printf("\t Degree stays the same\n");
                    	break;
                	}

            		headcount=headcount->next;
        	}

    		//The +y means going down, -y means going up
    		CvPoint boxcenter=cvPoint(box_para[idx_store[p]][0]+(box_para[idx_store[p]][2]/2), box_para[idx_store[p]][1]+(box_para[idx_store[p]][3])/2);
    		CvPoint boxvelocity=cvPoint(average_result.magnitude*cos(average_result.degree*3.1415926/180), -(average_result.magnitude*sin(average_result.degree*3.1415926/180)));


        	if(match || (match2 && overlap)){

        		//if matched, update the each kalman filter in the hashtable
        		printf("3. Kalman Filter Update: \n");
        		DataItem* temp_DataItem=hashsearch(hashArray, box_para[idx_store[p]][9]);
        		temp_kalmanbox=temp_DataItem->element;
        		CvMat* y_k=update_kalmanfilter(im_frame, temp_kalmanbox, boxcenter, boxvelocity, box_para[idx_store[p]][2], box_para[idx_store[p]][3]);

        		hashUpdate(hashArray, box_para[idx_store[p]][9], temp_kalmanbox);

//        		//TODO: Use the prediction infomation and optical flow vector
//        		if(frame_num>debug_frame&&box_para[idx_store[p]][9]==5){
//
//        			printf("4. Prob Bumping: \n");
//           			printf("\t HERE! prob: %i index: %i \n", box_para[idx_store[p]][5], idx_store[p]);
//        			float** probsMore=getProbsMore(1);
//        			int ctbumping=lookAround(probsMore, probs, num, idx_store[p], box_para[idx_store[p]][4], box_para[idx_store[p]][5], thresh, average_result.degree);
//        			printf("\t Object %i is moving: %i\n",box_para[idx_store[p]][9], ctbumping);
//
//
//        		}
        		//Save the matched Boxflow into a temporary array
         		Boxflow temp_boxflow=putFlowInsideBox(average_result,box_para[idx_store[p]][0], box_para[idx_store[p]][1], box_para[idx_store[p]][2], box_para[idx_store[p]][3], box_para[idx_store[p]][4], box_para[idx_store[p]][5], box_para[idx_store[p]][6], box_para[idx_store[p]][7], box_para[idx_store[p]][8], box_para[idx_store[p]][9]);
        		box_tempfull[idx_store[p]]=temp_boxflow;

        	}
        	else{
        		//if not matched, put the new kalman filter inside the hashtable
        		box_tempfull[idx_store[p]]=putFlowInsideBox(average_result,box_para[idx_store[p]][0], box_para[idx_store[p]][1], box_para[idx_store[p]][2], box_para[idx_store[p]][3], box_para[idx_store[p]][4], box_para[idx_store[p]][5], box_para[idx_store[p]][6], box_para[idx_store[p]][7], box_para[idx_store[p]][8], objectIndex);
        		printf("\t New object: %i\n", objectIndex);

//        		if(objectIndex==14){
//        			probs[idx_store[p]][0]=1;
//        			probs[idx_store[p]+676][0]=1;
//        			probs[idx_store[p]+676*2][0]=1;
//        			probs[idx_store[p]+676*3][0]=1;
//        			probs[idx_store[p]+676*4][0]=1;
//
//        		}
        		printf("3. Kalman Filter Initilization: \n");
        		temp_kalmanbox=create_kalmanfilter(boxcenter, boxvelocity);
        		hashinsert(hashArray, objectIndex, temp_kalmanbox);
        		objectIndex=objectIndex+1;

        	}


        	//drawArrow(im_frame, average_result.abs_p0, average_result.abs_p1, CV_RGB(box_para[idx_store[p]][10], box_para[idx_store[p]][11], box_para[idx_store[p]][12]), 10, 2, 9, 0);
        	draw_tracking(im_frame, box_para[idx_store[p]][0], box_para[idx_store[p]][1], box_para[idx_store[p]][2], box_para[idx_store[p]][3], box_para[idx_store[p]][10], box_para[idx_store[p]][11], box_para[idx_store[p]][12], box_tempfull[idx_store[p]].objectIndex);

            if(frame_num>=debug_frame){
            	cvWaitKey(0);
            }

            idx_tempprestore[p]=idx_store[p]; //ADDED: store the index of bouding boxes

        	cvReleaseImage(&pre_boxcrop);
        	cvReleaseImage(&boxcrop);
    	}

    	hashdisplay(hashArray);

    	//Draw any unmatched objects from previous frames
    	if(box_Adfull[0].objectIndex==5){
    		saveUnmatched(im_frame, box_Adfull[0]);
    	}

    	printf("6. Clear: \n");
    	if (object_prenum!=0){
    		snode* headcount=headconstant;
    		while(headcount!=NULL){
        		int headnumber=headcount->data;
        		Boxflow nullflow=putNullInsideBox();


        		if(box_full[headnumber].objectIndex==5){
        			saveUnmatched(im_frame, box_full[headnumber]);
        		}

        		else{
        			printf("\t %i in box_full does not have any match\n", headnumber);
        		}

        		box_full[headnumber]=nullflow;
        		headcount=headcount->next;
    		}
    		dispose(headconstant);
    	}



    	//TODO: Optimize later
    	int pp;
      	for(pp=0;pp<object_num;pp++){
      		printf("%i\n",idx_store[pp]);
    		box_full[idx_store[pp]]=box_tempfull[idx_store[pp]];
    	}


		snode* head=NULL;
		head=prepend(head, 0);
		headconstant=head;

    	int xx;
    	for(xx=0;xx<40;xx++){

    		if(idx_tempprestore[xx]!=0){
    			printf("idx_tempprestore[xx] %i\n",idx_tempprestore[xx]);
    			head=append(head,idx_tempprestore[xx]);
    		}

    		idx_store[xx]=0;
    		idx_tempprestore[xx]=0;


    	}


        if(saveOpticalflow){
        	CvSize size;{size.width = im_frame->width, size.height = im_frame->height;}
        	static CvVideoWriter* opticalflow_video = NULL;
        	if (opticalflow_video == NULL)
        	{
        		const char* output_name = "opticalflow.avi";
        		opticalflow_video = cvCreateVideoWriter(output_name, CV_FOURCC('D', 'I', 'V', 'X'), 5, size, 1);
        	}
        	cvWriteFrame(opticalflow_video, im_frame);
        }
        cvReleaseImage(&im_frame);
        object_prenum=object_num;
        object_num=0;

    }




    printf("7. Detection: \n");
    int j;
    for(j=0; j<1; j++){
        int left  = box_Adfull[j].left;
        int top   = box_Adfull[j].top;
        int right = box_Adfull[j].left+box_Adfull[j].width;
        int bot   = box_Adfull[j].top+box_Adfull[j].height;
        int width = im.h * .012*0.25;

        draw_box_width(im, left, top, right, bot, width, 0.5, 0.5, 0.5);
    }

    int objectIndex2=0;
    for(i = 0; i < num; ++i){
        int class = max_index(probs[i], classes);
        float prob = probs[i][class];

        if(prob > thresh){

        	//width determines the thickness of the bounding boxes
            int width = im.h * .012*0.25;

            if(0){
                width = pow(prob, 1./2.)*10+1;
                alphabet = 0;
            }

            int offset = class*123457 % classes;
            float red = get_color(2,offset,classes);
            float green = get_color(1,offset,classes);
            float blue = get_color(0,offset,classes);
            float rgb[3];


            rgb[0] = red;
            rgb[1] = green;
            rgb[2] = blue;
            box b = boxes[i];

            int left  = (b.x-b.w/2.)*im.w;
            int right = (b.x+b.w/2.)*im.w;
            int top   = (b.y-b.h/2.)*im.h;
            int bot   = (b.y+b.h/2.)*im.h;

            if(left < 0) left = 0;
            if(right > im.w-1) right = im.w-1;
            if(top < 0) top = 0;
            if(bot > im.h-1) bot = im.h-1;


            //Export the cell information to variables
            float row=probs[i][81];
            float col=probs[i][82];
            float nn=probs[i][83];


            char fr[sizeof(frame_num)];
            sprintf(fr, "%d", frame_num);
            char rw[sizeof(row)];
            sprintf(rw, "%0.0f", row);
            char cl[sizeof(col)];
            sprintf(cl,"%0.0f", col);
            char n[sizeof(nn)];
            sprintf(n,"%0.0f", nn);
            char obj[sizeof(objectIndex2)];
            sprintf(obj,"%d", objectIndex2);


//            char classtype[sizeof(names[class])];
//            sprintf(classtype, "%s", names[class]);

            box_para[i][0]=left;
            box_para[i][1]=top;
            box_para[i][2]=(right-left);
            box_para[i][3]=(bot-top);
            box_para[i][4]=class;
            box_para[i][5]=prob*100;
            box_para[i][6]=(int)row;
            box_para[i][7]=(int)col;
            box_para[i][8]=(int)nn;
            //box_para[i][9]=objectIndex2;

            int RED=cvRound(red*255);
            int GREEN=cvRound(green*255);
            int BLUE=cvRound(blue*255);

            box_para[i][10]=RED;
            box_para[i][11]=GREEN;
            box_para[i][12]=BLUE;

            idx_store[idx_count]=i;
            idx_count=idx_count+1;
            object_num=object_num+1;


            printf("\t Frame: %s Class: %s %0.f%%, index: %i, row: %0.0f, col: %0.0f, n:%0.0f objectIndex: %d\n", fr, names[class], prob*100, i, probs[i][81], probs[i][82], probs[i][83], objectIndex2);
            draw_box_width(im, left, top, right, bot, width, red, green, blue);
//            if (alphabet) {
//            	//Print the frame number, bbox number, object number to each label
//            	char label_frame_bbox[sizeof(fr)+sizeof(names[class])+sizeof(row)+sizeof(cl)+sizeof(n)+sizeof(obj)];
//                strcpy( label_frame_bbox, names[class] );
//                strcat( label_frame_bbox, "_" );
//                strcat( label_frame_bbox, fr );
//                strcat( label_frame_bbox, "_" );
//                strcat( label_frame_bbox, rw );
//                strcat( label_frame_bbox, "_" );
//                strcat( label_frame_bbox, cl );
//                strcat( label_frame_bbox, "_" );
//                strcat( label_frame_bbox, n );
//                strcat( label_frame_bbox, "_" );
//                strcat( label_frame_bbox, obj );
//
//                image label = get_label(alphabet, label_frame_bbox, (im.h*.03*0.5)/10);
//                draw_label(im, top + width, left, label, rgb);
//                free_image(label);
//            }

        }
        	objectIndex2=objectIndex2+1;
    }
    free_image(pre_im);
    pre_im=copy_image(screenshot);
    free_image(screenshot);
    frame_num=frame_num+1;


}

void transpose_image(image im)
{
    assert(im.w == im.h);
    int n, m;
    int c;
    for(c = 0; c < im.c; ++c){
        for(n = 0; n < im.w-1; ++n){
            for(m = n + 1; m < im.w; ++m){
                float swap = im.data[m + im.w*(n + im.h*c)];
                im.data[m + im.w*(n + im.h*c)] = im.data[n + im.w*(m + im.h*c)];
                im.data[n + im.w*(m + im.h*c)] = swap;
            }
        }
    }
}

void rotate_image_cw(image im, int times)
{
    assert(im.w == im.h);
    times = (times + 400) % 4;
    int i, x, y, c;
    int n = im.w;
    for(i = 0; i < times; ++i){
        for(c = 0; c < im.c; ++c){
            for(x = 0; x < n/2; ++x){
                for(y = 0; y < (n-1)/2 + 1; ++y){
                    float temp = im.data[y + im.w*(x + im.h*c)];
                    im.data[y + im.w*(x + im.h*c)] = im.data[n-1-x + im.w*(y + im.h*c)];
                    im.data[n-1-x + im.w*(y + im.h*c)] = im.data[n-1-y + im.w*(n-1-x + im.h*c)];
                    im.data[n-1-y + im.w*(n-1-x + im.h*c)] = im.data[x + im.w*(n-1-y + im.h*c)];
                    im.data[x + im.w*(n-1-y + im.h*c)] = temp;
                }
            }
        }
    }
}

void flip_image(image a)
{
    int i,j,k;
    for(k = 0; k < a.c; ++k){
        for(i = 0; i < a.h; ++i){
            for(j = 0; j < a.w/2; ++j){
                int index = j + a.w*(i + a.h*(k));
                int flip = (a.w - j - 1) + a.w*(i + a.h*(k));
                float swap = a.data[flip];
                a.data[flip] = a.data[index];
                a.data[index] = swap;
            }
        }
    }
}

image image_distance(image a, image b)
{
    int i,j;
    image dist = make_image(a.w, a.h, 1);
    for(i = 0; i < a.c; ++i){
        for(j = 0; j < a.h*a.w; ++j){
            dist.data[j] += pow(a.data[i*a.h*a.w+j]-b.data[i*a.h*a.w+j],2);
        }
    }
    for(j = 0; j < a.h*a.w; ++j){
        dist.data[j] = sqrt(dist.data[j]);
    }
    return dist;
}

void ghost_image(image source, image dest, int dx, int dy)
{
    int x,y,k;
    float max_dist = sqrt((-source.w/2. + .5)*(-source.w/2. + .5));
    for(k = 0; k < source.c; ++k){
        for(y = 0; y < source.h; ++y){
            for(x = 0; x < source.w; ++x){
                float dist = sqrt((x - source.w/2. + .5)*(x - source.w/2. + .5) + (y - source.h/2. + .5)*(y - source.h/2. + .5));
                float alpha = (1 - dist/max_dist);
                if(alpha < 0) alpha = 0;
                float v1 = get_pixel(source, x,y,k);
                float v2 = get_pixel(dest, dx+x,dy+y,k);
                float val = alpha*v1 + (1-alpha)*v2;
                set_pixel(dest, dx+x, dy+y, k, val);
            }
        }
    }
}

void embed_image(image source, image dest, int dx, int dy)
{
    int x,y,k;
    for(k = 0; k < source.c; ++k){
        for(y = 0; y < source.h; ++y){
            for(x = 0; x < source.w; ++x){
                float val = get_pixel(source, x,y,k);
                set_pixel(dest, dx+x, dy+y, k, val);
            }
        }
    }
}

image collapse_image_layers(image source, int border)
{
    int h = source.h;
    h = (h+border)*source.c - border;
    image dest = make_image(source.w, h, 1);
    int i;
    for(i = 0; i < source.c; ++i){
        image layer = get_image_layer(source, i);
        int h_offset = i*(source.h+border);
        embed_image(layer, dest, 0, h_offset);
        free_image(layer);
    }
    return dest;
}

void constrain_image(image im)
{
    int i;
    for(i = 0; i < im.w*im.h*im.c; ++i){
        if(im.data[i] < 0) im.data[i] = 0;
        if(im.data[i] > 1) im.data[i] = 1;
    }
}

void normalize_image(image p)
{
    int i;
    float min = 9999999;
    float max = -999999;

    for(i = 0; i < p.h*p.w*p.c; ++i){
        float v = p.data[i];
        if(v < min) min = v;
        if(v > max) max = v;
    }
    if(max - min < .000000001){
        min = 0;
        max = 1;
    }
    for(i = 0; i < p.c*p.w*p.h; ++i){
        p.data[i] = (p.data[i] - min)/(max-min);
    }
}

void normalize_image2(image p)
{
    float *min = calloc(p.c, sizeof(float));
    float *max = calloc(p.c, sizeof(float));
    int i,j;
    for(i = 0; i < p.c; ++i) min[i] = max[i] = p.data[i*p.h*p.w];

    for(j = 0; j < p.c; ++j){
        for(i = 0; i < p.h*p.w; ++i){
            float v = p.data[i+j*p.h*p.w];
            if(v < min[j]) min[j] = v;
            if(v > max[j]) max[j] = v;
        }
    }
    for(i = 0; i < p.c; ++i){
        if(max[i] - min[i] < .000000001){
            min[i] = 0;
            max[i] = 1;
        }
    }
    for(j = 0; j < p.c; ++j){
        for(i = 0; i < p.w*p.h; ++i){
            p.data[i+j*p.h*p.w] = (p.data[i+j*p.h*p.w] - min[j])/(max[j]-min[j]);
        }
    }
    free(min);
    free(max);
}

void copy_image_into(image src, image dest)
{
    memcpy(dest.data, src.data, src.h*src.w*src.c*sizeof(float));
}

image copy_image(image p)
{
    image copy = p;
    copy.data = calloc(p.h*p.w*p.c, sizeof(float));
    memcpy(copy.data, p.data, p.h*p.w*p.c*sizeof(float));
    return copy;
}

void rgbgr_image(image im)
{
    int i;
    for(i = 0; i < im.w*im.h; ++i){
        float swap = im.data[i];
        im.data[i] = im.data[i+im.w*im.h*2];
        im.data[i+im.w*im.h*2] = swap;
    }
}

//ADDED: Draw arrow on the image REF: http://mlikihazar.blogspot.ca/2013/02/draw-arrow-opencv.html
void drawArrow(IplImage *image, CvPoint p, CvPoint q, CvScalar color, int arrowMagnitude, int thickness, int line_type, int shift)
{
    //Draw the principle line

    cvLine(image, p, q, color, thickness, line_type, shift);
    const double PI = 3.141592653;
    //compute the angle alpha
    double angle = atan2((double)p.y-q.y, (double)p.x-q.x);
    //compute the coordinates of the first segment
    p.x = (int) ( q.x +  arrowMagnitude * cos(angle + PI/8));
    p.y = (int) ( q.y +  arrowMagnitude * sin(angle + PI/8));
    //Draw the first segment
    cvLine(image, p, q, color, thickness, line_type, shift);
    //compute the coordinates of the second segment
    p.x = (int) ( q.x +  arrowMagnitude * cos(angle - PI/8));
    p.y = (int) ( q.y +  arrowMagnitude * sin(angle - PI/8));


    //Draw the second segment
    cvLine(image, p, q, color, thickness, line_type, shift);

}


//ADDED: Convert image object to IplImage object
IplImage* image_convert_IplImage(image p, IplImage *disp){
    int step = disp->widthStep;
    int x,y,k;
    for(y = 0; y < p.h; ++y){
        for(x = 0; x < p.w; ++x){
            for(k= 0; k < p.c; ++k){
                disp->imageData[y*step + x*p.c + k] = (unsigned char)(get_pixel(p,x,y,k)*255);
            }
        }
    }

	return disp;

}


Boxflow putFlowInsideBox(Opticalflow vector, int left, int top, int width, int height, int classtype, float prob, int row, int col, int nn, int objectIndex){
	Boxflow out;
	out.flow=vector;
	out.left=left;
	out.top=top;
	out.width=width;
	out.height=height;
	out.classtype=classtype;
	out.prob=prob;
	out.row=row;
	out.col=col;
	out.nn=nn;
	out.objectIndex=objectIndex;

	return out;
}

Boxflow putNullInsideBox(){
	Boxflow out;
	Opticalflow vector=create_opticalflow(cvPoint(0,0), cvPoint(0,0), cvPoint(0,0), cvPoint(0,0));
	out.flow=vector;
	out.left=0;
	out.top=0;
	out.width=0;
	out.height=0;
	out.classtype=0;
	out.prob=0;
	out.row=0;
	out.col=0;
	out.nn=0;
	out.objectIndex=0;

	return out;
}


#ifdef OPENCV
void show_image_cv(image p, const char *name, IplImage *disp)
{
    int x,y,k;
    if(p.c == 3) rgbgr_image(p);
    //normalize_image(copy);

    char buff[256];
    //sprintf(buff, "%s (%d)", name, windows);
    sprintf(buff, "%s", name);

    int step = disp->widthStep;
    cvNamedWindow(buff, CV_WINDOW_NORMAL); 
    //cvMoveWindow(buff, 100*(windows%10) + 200*(windows/10), 100*(windows%10));
    ++windows;
    for(y = 0; y < p.h; ++y){
        for(x = 0; x < p.w; ++x){
            for(k= 0; k < p.c; ++k){
                disp->imageData[y*step + x*p.c + k] = (unsigned char)(get_pixel(p,x,y,k)*255);
            }
        }
    }
    if(0){
        int w = 448;
        int h = w*p.h/p.w;
        if(h > 1000){
            h = 1000;
            w = h*p.w/p.h;
        }
        IplImage *buffer = disp;
        disp = cvCreateImage(cvSize(w, h), buffer->depth, buffer->nChannels);
        cvResize(buffer, disp, CV_INTER_LINEAR);
        cvReleaseImage(&buffer);
    }

    cvShowImage(buff, disp);

    //ADDED: Save the demo input video to output
    if(saveDetection){
    	CvSize size;{size.width = disp->width, size.height = disp->height;}
    	static CvVideoWriter* output_video = NULL;
    	if (output_video == NULL)
    	{
    		const char* output_name = "detection.avi";
    		output_video = cvCreateVideoWriter(output_name, CV_FOURCC('D', 'I', 'V', 'X'), 25, size, 1);
    	}
    	cvWriteFrame(output_video, disp);
    }

}
#endif

void show_image(image p, const char *name)
{
#ifdef OPENCV
    IplImage *disp = cvCreateImage(cvSize(p.w,p.h), IPL_DEPTH_8U, p.c);
    image copy = copy_image(p);
    constrain_image(copy);
    show_image_cv(copy, name, disp);
    free_image(copy);
    cvReleaseImage(&disp);
#else
    fprintf(stderr, "Not compiled with OpenCV, saving to %s.png instead\n", name);
    save_image(p, name);
#endif
}

#ifdef OPENCV

void ipl_into_image(IplImage* src, image im)
{
    unsigned char *data = (unsigned char *)src->imageData;
    int h = src->height;
    int w = src->width;
    int c = src->nChannels;
    int step = src->widthStep;
    int i, j, k;

    for(i = 0; i < h; ++i){
        for(k= 0; k < c; ++k){
            for(j = 0; j < w; ++j){
                im.data[k*w*h + i*w + j] = data[i*step + j*c + k]/255.;
            }
        }
    }
}

image ipl_to_image(IplImage* src)
{
    int h = src->height;
    int w = src->width;
    int c = src->nChannels;
    image out = make_image(w, h, c);
    ipl_into_image(src, out);
    return out;
}

image load_image_cv(char *filename, int channels)
{
    IplImage* src = 0;
    int flag = -1;
    if (channels == 0) flag = -1;
    else if (channels == 1) flag = 0;
    else if (channels == 3) flag = 1;
    else {
        fprintf(stderr, "OpenCV can't force load with %d channels\n", channels);
    }

    if( (src = cvLoadImage(filename, flag)) == 0 )
    {
        fprintf(stderr, "Cannot load image \"%s\"\n", filename);
        char buff[256];
        sprintf(buff, "echo %s >> bad.list", filename);
        system(buff);
        return make_image(10,10,3);
        //exit(0);
    }
    image out = ipl_to_image(src);
    cvReleaseImage(&src);
    rgbgr_image(out);
    return out;
}

void flush_stream_buffer(CvCapture *cap, int n)
{
    int i;
    for(i = 0; i < n; ++i) {
        cvQueryFrame(cap);
    }
}

image get_image_from_stream(CvCapture *cap)
{
    IplImage* src = cvQueryFrame(cap);
    if (!src) return make_empty_image(0,0,0);
    image im = ipl_to_image(src);
    rgbgr_image(im);
    return im;
}

int fill_image_from_stream(CvCapture *cap, image im)
{
    IplImage* src = cvQueryFrame(cap);
    if (!src) return 0;
    ipl_into_image(src, im);
    rgbgr_image(im);
    return 1;
}

void save_image_jpg(image p, const char *name)
{
    image copy = copy_image(p);
    if(p.c == 3) rgbgr_image(copy);
    int x,y,k;

    char buff[256];
    sprintf(buff, "%s.jpg", name);

    IplImage *disp = cvCreateImage(cvSize(p.w,p.h), IPL_DEPTH_8U, p.c);
    int step = disp->widthStep;
    for(y = 0; y < p.h; ++y){
        for(x = 0; x < p.w; ++x){
            for(k= 0; k < p.c; ++k){
                disp->imageData[y*step + x*p.c + k] = (unsigned char)(get_pixel(copy,x,y,k)*255);
            }
        }
    }
    cvSaveImage(buff, disp,0);
    cvReleaseImage(&disp);
    free_image(copy);
}
#endif

void save_image_png(image im, const char *name)
{
    char buff[256];
    //sprintf(buff, "%s (%d)", name, windows);
    sprintf(buff, "%s.png", name);
    unsigned char *data = calloc(im.w*im.h*im.c, sizeof(char));
    int i,k;
    for(k = 0; k < im.c; ++k){
        for(i = 0; i < im.w*im.h; ++i){
            data[i*im.c+k] = (unsigned char) (255*im.data[i + k*im.w*im.h]);
        }
    }
    int success = stbi_write_png(buff, im.w, im.h, im.c, data, im.w*im.c);
    free(data);
    if(!success) fprintf(stderr, "Failed to write image %s\n", buff);
}

void save_image(image im, const char *name)
{
#ifdef OPENCV
    save_image_jpg(im, name);
#else
    save_image_png(im, name);
#endif
}


void show_image_layers(image p, char *name)
{
    int i;
    char buff[256];
    for(i = 0; i < p.c; ++i){
        sprintf(buff, "%s - Layer %d", name, i);
        image layer = get_image_layer(p, i);
        show_image(layer, buff);
        free_image(layer);
    }
}

void show_image_collapsed(image p, char *name)
{
    image c = collapse_image_layers(p, 1);
    show_image(c, name);
    free_image(c);
}

image make_empty_image(int w, int h, int c)
{
    image out;
    out.data = 0;
    out.h = h;
    out.w = w;
    out.c = c;
    return out;
}

int computeDegree(double sum_p0_x, double sum_p0_y, double sum_p1_x, double sum_p1_y){
	if(sum_p0_x==sum_p1_x && sum_p0_y==sum_p1_y){
		return 0;
	}
    int degree=atan((sum_p1_y-sum_p0_y)/(sum_p1_x-sum_p0_x))*(180.0/3.1415926);
    if (degree>0){
    	if (sum_p1_x>sum_p0_x){
    		degree=360-degree;
    	}
    	else if (sum_p1_x<sum_p0_x){
    		degree=180-degree;
    	}
    	else{
    		degree=270;
    	}
    }

    else if (degree<0){
    	if (sum_p1_y<sum_p0_y){
    		degree=-degree;
    	}
    	else if(sum_p1_x<sum_p0_x){
    		degree=180-degree;
    	}
    	else{
    		degree=90;
    	}

    }
    else{
    	if (sum_p1_x>sum_p0_x){
    		degree=0;
    	}
    	else if (sum_p1_x<sum_p0_x){
    		degree=180;

    	}
    }
    return degree;
}

int computeMagnitude(double sum_p0_x, double sum_p0_y, double sum_p1_x, double sum_p1_y){
	int magnitude=sqrt(pow(abs(sum_p0_x-sum_p1_x), 2)+pow(abs(sum_p0_y-sum_p1_y),2));
	return magnitude;
}




image make_image(int w, int h, int c)
{
    image out = make_empty_image(w,h,c);
    out.data = calloc(h*w*c, sizeof(float));
    return out;
}

image make_random_image(int w, int h, int c)
{
    image out = make_empty_image(w,h,c);
    out.data = calloc(h*w*c, sizeof(float));
    int i;
    for(i = 0; i < w*h*c; ++i){
        out.data[i] = (rand_normal() * .25) + .5;
    }
    return out;
}

image float_to_image(int w, int h, int c, float *data)
{
    image out = make_empty_image(w,h,c);
    out.data = data;
    return out;
}

void place_image(image im, int w, int h, int dx, int dy, image canvas)
{
    int x, y, c;
    for(c = 0; c < im.c; ++c){
        for(y = 0; y < h; ++y){
            for(x = 0; x < w; ++x){
                int rx = ((float)x / w) * im.w;
                int ry = ((float)y / h) * im.h;
                float val = bilinear_interpolate(im, rx, ry, c);
                set_pixel(canvas, x + dx, y + dy, c, val);
            }
        }
    }
}

image center_crop_image(image im, int w, int h)
{
    int m = (im.w < im.h) ? im.w : im.h;   
    image c = crop_image(im, (im.w - m) / 2, (im.h - m)/2, m, m);
    image r = resize_image(c, w, h);
    free_image(c);
    return r;
}

image rotate_crop_image(image im, float rad, float s, int w, int h, float dx, float dy, float aspect)
{
    int x, y, c;
    float cx = im.w/2.;
    float cy = im.h/2.;
    image rot = make_image(w, h, im.c);
    for(c = 0; c < im.c; ++c){
        for(y = 0; y < h; ++y){
            for(x = 0; x < w; ++x){
                float rx = cos(rad)*((x - w/2.)/s*aspect + dx/s*aspect) - sin(rad)*((y - h/2.)/s + dy/s) + cx;
                float ry = sin(rad)*((x - w/2.)/s*aspect + dx/s*aspect) + cos(rad)*((y - h/2.)/s + dy/s) + cy;
                float val = bilinear_interpolate(im, rx, ry, c);
                set_pixel(rot, x, y, c, val);
            }
        }
    }
    return rot;
}

image rotate_image(image im, float rad)
{
    int x, y, c;
    float cx = im.w/2.;
    float cy = im.h/2.;
    image rot = make_image(im.w, im.h, im.c);
    for(c = 0; c < im.c; ++c){
        for(y = 0; y < im.h; ++y){
            for(x = 0; x < im.w; ++x){
                float rx = cos(rad)*(x-cx) - sin(rad)*(y-cy) + cx;
                float ry = sin(rad)*(x-cx) + cos(rad)*(y-cy) + cy;
                float val = bilinear_interpolate(im, rx, ry, c);
                set_pixel(rot, x, y, c, val);
            }
        }
    }
    return rot;
}

void fill_image(image m, float s)
{
    int i;
    for(i = 0; i < m.h*m.w*m.c; ++i) m.data[i] = s;
}

void translate_image(image m, float s)
{
    int i;
    for(i = 0; i < m.h*m.w*m.c; ++i) m.data[i] += s;
}

void scale_image(image m, float s)
{
    int i;
    for(i = 0; i < m.h*m.w*m.c; ++i) m.data[i] *= s;
}

image crop_image(image im, int dx, int dy, int w, int h)
{
    image cropped = make_image(w, h, im.c);
    int i, j, k;
    for(k = 0; k < im.c; ++k){
        for(j = 0; j < h; ++j){
            for(i = 0; i < w; ++i){
                int r = j + dy;
                int c = i + dx;
                float val = 0;
                r = constrain_int(r, 0, im.h-1);
                c = constrain_int(c, 0, im.w-1);
                val = get_pixel(im, c, r, k);
                set_pixel(cropped, i, j, k, val);
            }
        }
    }
    return cropped;
}

int best_3d_shift_r(image a, image b, int min, int max)
{
    if(min == max) return min;
    int mid = floor((min + max) / 2.);
    image c1 = crop_image(b, 0, mid, b.w, b.h);
    image c2 = crop_image(b, 0, mid+1, b.w, b.h);
    float d1 = dist_array(c1.data, a.data, a.w*a.h*a.c, 10);
    float d2 = dist_array(c2.data, a.data, a.w*a.h*a.c, 10);
    free_image(c1);
    free_image(c2);
    if(d1 < d2) return best_3d_shift_r(a, b, min, mid);
    else return best_3d_shift_r(a, b, mid+1, max);
}

int best_3d_shift(image a, image b, int min, int max)
{
    int i;
    int best = 0;
    float best_distance = FLT_MAX;
    for(i = min; i <= max; i += 2){
        image c = crop_image(b, 0, i, b.w, b.h);
        float d = dist_array(c.data, a.data, a.w*a.h*a.c, 100);
        if(d < best_distance){
            best_distance = d;
            best = i;
        }
        printf("%d %f\n", i, d);
        free_image(c);
    }
    return best;
}

void composite_3d(char *f1, char *f2, char *out, int delta)
{
    if(!out) out = "out";
    image a = load_image(f1, 0,0,0);
    image b = load_image(f2, 0,0,0);
    int shift = best_3d_shift_r(a, b, -a.h/100, a.h/100);

    image c1 = crop_image(b, 10, shift, b.w, b.h);
    float d1 = dist_array(c1.data, a.data, a.w*a.h*a.c, 100);
    image c2 = crop_image(b, -10, shift, b.w, b.h);
    float d2 = dist_array(c2.data, a.data, a.w*a.h*a.c, 100);

    if(d2 < d1 && 0){
        image swap = a;
        a = b;
        b = swap;
        shift = -shift;
        printf("swapped, %d\n", shift);
    }
    else{
        printf("%d\n", shift);
    }

    image c = crop_image(b, delta, shift, a.w, a.h);
    int i;
    for(i = 0; i < c.w*c.h; ++i){
        c.data[i] = a.data[i];
    }
#ifdef OPENCV
    save_image_jpg(c, out);
#else
    save_image(c, out);
#endif
}

void letterbox_image_into(image im, int w, int h, image boxed)
{
    int new_w = im.w;
    int new_h = im.h;
    if (((float)w/im.w) < ((float)h/im.h)) {
        new_w = w;
        new_h = (im.h * w)/im.w;
    } else {
        new_h = h;
        new_w = (im.w * h)/im.h;
    }
    image resized = resize_image(im, new_w, new_h);
    embed_image(resized, boxed, (w-new_w)/2, (h-new_h)/2); 
    free_image(resized);
}

image letterbox_image(image im, int w, int h)
{
    int new_w = im.w;
    int new_h = im.h;
    if (((float)w/im.w) < ((float)h/im.h)) {
        new_w = w;
        new_h = (im.h * w)/im.w;
    } else {
        new_h = h;
        new_w = (im.w * h)/im.h;
    }
    image resized = resize_image(im, new_w, new_h);
    image boxed = make_image(w, h, im.c);
    fill_image(boxed, .5);
    //int i;
    //for(i = 0; i < boxed.w*boxed.h*boxed.c; ++i) boxed.data[i] = 0;
    embed_image(resized, boxed, (w-new_w)/2, (h-new_h)/2); 
    free_image(resized);
    return boxed;
}

image resize_max(image im, int max)
{
    int w = im.w;
    int h = im.h;
    if(w > h){
        h = (h * max) / w;
        w = max;
    } else {
        w = (w * max) / h;
        h = max;
    }
    if(w == im.w && h == im.h) return im;
    image resized = resize_image(im, w, h);
    return resized;
}

image resize_min(image im, int min)
{
    int w = im.w;
    int h = im.h;
    if(w < h){
        h = (h * min) / w;
        w = min;
    } else {
        w = (w * min) / h;
        h = min;
    }
    if(w == im.w && h == im.h) return im;
    image resized = resize_image(im, w, h);
    return resized;
}

image random_crop_image(image im, int w, int h)
{
    int dx = rand_int(0, im.w - w);
    int dy = rand_int(0, im.h - h);
    image crop = crop_image(im, dx, dy, w, h);
    return crop;
}

image random_augment_image(image im, float angle, float aspect, int low, int high, int size)
{
    aspect = rand_scale(aspect);
    int r = rand_int(low, high);
    int min = (im.h < im.w*aspect) ? im.h : im.w*aspect;
    float scale = (float)r / min;

    float rad = rand_uniform(-angle, angle) * TWO_PI / 360.;

    float dx = (im.w*scale/aspect - size) / 2.;
    float dy = (im.h*scale - size) / 2.;
    if(dx < 0) dx = 0;
    if(dy < 0) dy = 0;
    dx = rand_uniform(-dx, dx);
    dy = rand_uniform(-dy, dy);

    image crop = rotate_crop_image(im, rad, scale, size, size, dx, dy, aspect);

    return crop;
}

float three_way_max(float a, float b, float c)
{
    return (a > b) ? ( (a > c) ? a : c) : ( (b > c) ? b : c) ;
}

float three_way_min(float a, float b, float c)
{
    return (a < b) ? ( (a < c) ? a : c) : ( (b < c) ? b : c) ;
}

void yuv_to_rgb(image im)
{
    assert(im.c == 3);
    int i, j;
    float r, g, b;
    float y, u, v;
    for(j = 0; j < im.h; ++j){
        for(i = 0; i < im.w; ++i){
            y = get_pixel(im, i , j, 0);
            u = get_pixel(im, i , j, 1);
            v = get_pixel(im, i , j, 2);

            r = y + 1.13983*v;
            g = y + -.39465*u + -.58060*v;
            b = y + 2.03211*u;

            set_pixel(im, i, j, 0, r);
            set_pixel(im, i, j, 1, g);
            set_pixel(im, i, j, 2, b);
        }
    }
}

void rgb_to_yuv(image im)
{
    assert(im.c == 3);
    int i, j;
    float r, g, b;
    float y, u, v;
    for(j = 0; j < im.h; ++j){
        for(i = 0; i < im.w; ++i){
            r = get_pixel(im, i , j, 0);
            g = get_pixel(im, i , j, 1);
            b = get_pixel(im, i , j, 2);

            y = .299*r + .587*g + .114*b;
            u = -.14713*r + -.28886*g + .436*b;
            v = .615*r + -.51499*g + -.10001*b;

            set_pixel(im, i, j, 0, y);
            set_pixel(im, i, j, 1, u);
            set_pixel(im, i, j, 2, v);
        }
    }
}

// http://www.cs.rit.edu/~ncs/color/t_convert.html
void rgb_to_hsv(image im)
{
    assert(im.c == 3);
    int i, j;
    float r, g, b;
    float h, s, v;
    for(j = 0; j < im.h; ++j){
        for(i = 0; i < im.w; ++i){
            r = get_pixel(im, i , j, 0);
            g = get_pixel(im, i , j, 1);
            b = get_pixel(im, i , j, 2);
            float max = three_way_max(r,g,b);
            float min = three_way_min(r,g,b);
            float delta = max - min;
            v = max;
            if(max == 0){
                s = 0;
                h = 0;
            }else{
                s = delta/max;
                if(r == max){
                    h = (g - b) / delta;
                } else if (g == max) {
                    h = 2 + (b - r) / delta;
                } else {
                    h = 4 + (r - g) / delta;
                }
                if (h < 0) h += 6;
                h = h/6.;
            }
            set_pixel(im, i, j, 0, h);
            set_pixel(im, i, j, 1, s);
            set_pixel(im, i, j, 2, v);
        }
    }
}

void hsv_to_rgb(image im)
{
    assert(im.c == 3);
    int i, j;
    float r, g, b;
    float h, s, v;
    float f, p, q, t;
    for(j = 0; j < im.h; ++j){
        for(i = 0; i < im.w; ++i){
            h = 6 * get_pixel(im, i , j, 0);
            s = get_pixel(im, i , j, 1);
            v = get_pixel(im, i , j, 2);
            if (s == 0) {
                r = g = b = v;
            } else {
                int index = floor(h);
                f = h - index;
                p = v*(1-s);
                q = v*(1-s*f);
                t = v*(1-s*(1-f));
                if(index == 0){
                    r = v; g = t; b = p;
                } else if(index == 1){
                    r = q; g = v; b = p;
                } else if(index == 2){
                    r = p; g = v; b = t;
                } else if(index == 3){
                    r = p; g = q; b = v;
                } else if(index == 4){
                    r = t; g = p; b = v;
                } else {
                    r = v; g = p; b = q;
                }
            }
            set_pixel(im, i, j, 0, r);
            set_pixel(im, i, j, 1, g);
            set_pixel(im, i, j, 2, b);
        }
    }
}

void grayscale_image_3c(image im)
{
    assert(im.c == 3);
    int i, j, k;
    float scale[] = {0.299, 0.587, 0.114};
    for(j = 0; j < im.h; ++j){
        for(i = 0; i < im.w; ++i){
            float val = 0;
            for(k = 0; k < 3; ++k){
                val += scale[k]*get_pixel(im, i, j, k);
            }
            im.data[0*im.h*im.w + im.w*j + i] = val;
            im.data[1*im.h*im.w + im.w*j + i] = val;
            im.data[2*im.h*im.w + im.w*j + i] = val;
        }
    }
}

image grayscale_image(image im)
{
    assert(im.c == 3);
    int i, j, k;
    image gray = make_image(im.w, im.h, 1);
    float scale[] = {0.299, 0.587, 0.114};
    for(k = 0; k < im.c; ++k){
        for(j = 0; j < im.h; ++j){
            for(i = 0; i < im.w; ++i){
                gray.data[i+im.w*j] += scale[k]*get_pixel(im, i, j, k);
            }
        }
    }
    return gray;
}

image threshold_image(image im, float thresh)
{
    int i;
    image t = make_image(im.w, im.h, im.c);
    for(i = 0; i < im.w*im.h*im.c; ++i){
        t.data[i] = im.data[i]>thresh ? 1 : 0;
    }
    return t;
}

image blend_image(image fore, image back, float alpha)
{
    assert(fore.w == back.w && fore.h == back.h && fore.c == back.c);
    image blend = make_image(fore.w, fore.h, fore.c);
    int i, j, k;
    for(k = 0; k < fore.c; ++k){
        for(j = 0; j < fore.h; ++j){
            for(i = 0; i < fore.w; ++i){
                float val = alpha * get_pixel(fore, i, j, k) + 
                    (1 - alpha)* get_pixel(back, i, j, k);
                set_pixel(blend, i, j, k, val);
            }
        }
    }
    return blend;
}

void scale_image_channel(image im, int c, float v)
{
    int i, j;
    for(j = 0; j < im.h; ++j){
        for(i = 0; i < im.w; ++i){
            float pix = get_pixel(im, i, j, c);
            pix = pix*v;
            set_pixel(im, i, j, c, pix);
        }
    }
}

void translate_image_channel(image im, int c, float v)
{
    int i, j;
    for(j = 0; j < im.h; ++j){
        for(i = 0; i < im.w; ++i){
            float pix = get_pixel(im, i, j, c);
            pix = pix+v;
            set_pixel(im, i, j, c, pix);
        }
    }
}

image binarize_image(image im)
{
    image c = copy_image(im);
    int i;
    for(i = 0; i < im.w * im.h * im.c; ++i){
        if(c.data[i] > .5) c.data[i] = 1;
        else c.data[i] = 0;
    }
    return c;
}

void saturate_image(image im, float sat)
{
    rgb_to_hsv(im);
    scale_image_channel(im, 1, sat);
    hsv_to_rgb(im);
    constrain_image(im);
}

void hue_image(image im, float hue)
{
    rgb_to_hsv(im);
    int i;
    for(i = 0; i < im.w*im.h; ++i){
        im.data[i] = im.data[i] + hue;
        if (im.data[i] > 1) im.data[i] -= 1;
        if (im.data[i] < 0) im.data[i] += 1;
    }
    hsv_to_rgb(im);
    constrain_image(im);
}

void exposure_image(image im, float sat)
{
    rgb_to_hsv(im);
    scale_image_channel(im, 2, sat);
    hsv_to_rgb(im);
    constrain_image(im);
}

void distort_image(image im, float hue, float sat, float val)
{
    rgb_to_hsv(im);
    scale_image_channel(im, 1, sat);
    scale_image_channel(im, 2, val);
    int i;
    for(i = 0; i < im.w*im.h; ++i){
        im.data[i] = im.data[i] + hue;
        if (im.data[i] > 1) im.data[i] -= 1;
        if (im.data[i] < 0) im.data[i] += 1;
    }
    hsv_to_rgb(im);
    constrain_image(im);
}

void random_distort_image(image im, float hue, float saturation, float exposure)
{
    float dhue = rand_uniform(-hue, hue);
    float dsat = rand_scale(saturation);
    float dexp = rand_scale(exposure);
    distort_image(im, dhue, dsat, dexp);
}

void saturate_exposure_image(image im, float sat, float exposure)
{
    rgb_to_hsv(im);
    scale_image_channel(im, 1, sat);
    scale_image_channel(im, 2, exposure);
    hsv_to_rgb(im);
    constrain_image(im);
}

float bilinear_interpolate(image im, float x, float y, int c)
{
    int ix = (int) floorf(x);
    int iy = (int) floorf(y);

    float dx = x - ix;
    float dy = y - iy;

    float val = (1-dy) * (1-dx) * get_pixel_extend(im, ix, iy, c) + 
        dy     * (1-dx) * get_pixel_extend(im, ix, iy+1, c) + 
        (1-dy) *   dx   * get_pixel_extend(im, ix+1, iy, c) +
        dy     *   dx   * get_pixel_extend(im, ix+1, iy+1, c);
    return val;
}

image resize_image(image im, int w, int h)
{
    image resized = make_image(w, h, im.c);   
    image part = make_image(w, im.h, im.c);
    int r, c, k;
    float w_scale = (float)(im.w - 1) / (w - 1);
    float h_scale = (float)(im.h - 1) / (h - 1);
    for(k = 0; k < im.c; ++k){
        for(r = 0; r < im.h; ++r){
            for(c = 0; c < w; ++c){
                float val = 0;
                if(c == w-1 || im.w == 1){
                    val = get_pixel(im, im.w-1, r, k);
                } else {
                    float sx = c*w_scale;
                    int ix = (int) sx;
                    float dx = sx - ix;
                    val = (1 - dx) * get_pixel(im, ix, r, k) + dx * get_pixel(im, ix+1, r, k);
                }
                set_pixel(part, c, r, k, val);
            }
        }
    }
    for(k = 0; k < im.c; ++k){
        for(r = 0; r < h; ++r){
            float sy = r*h_scale;
            int iy = (int) sy;
            float dy = sy - iy;
            for(c = 0; c < w; ++c){
                float val = (1-dy) * get_pixel(part, c, iy, k);
                set_pixel(resized, c, r, k, val);
            }
            if(r == h-1 || im.h == 1) continue;
            for(c = 0; c < w; ++c){
                float val = dy * get_pixel(part, c, iy+1, k);
                add_pixel(resized, c, r, k, val);
            }
        }
    }

    free_image(part);
    return resized;
}


void test_resize(char *filename)
{
    image im = load_image(filename, 0,0, 3);
    float mag = mag_array(im.data, im.w*im.h*im.c);
    printf("L2 Norm: %f\n", mag);
    image gray = grayscale_image(im);

    image c1 = copy_image(im);
    image c2 = copy_image(im);
    image c3 = copy_image(im);
    image c4 = copy_image(im);
    distort_image(c1, .1, 1.5, 1.5);
    distort_image(c2, -.1, .66666, .66666);
    distort_image(c3, .1, 1.5, .66666);
    distort_image(c4, .1, .66666, 1.5);


    show_image(im,   "Original");
    show_image(gray, "Gray");
    show_image(c1, "C1");
    show_image(c2, "C2");
    show_image(c3, "C3");
    show_image(c4, "C4");
#ifdef OPENCV
    while(1){
        image aug = random_augment_image(im, 0, .75, 320, 448, 320);
        show_image(aug, "aug");
        free_image(aug);


        float exposure = 1.15;
        float saturation = 1.15;
        float hue = .05;

        image c = copy_image(im);

        float dexp = rand_scale(exposure);
        float dsat = rand_scale(saturation);
        float dhue = rand_uniform(-hue, hue);

        distort_image(c, dhue, dsat, dexp);
        show_image(c, "rand");
        printf("%f %f %f\n", dhue, dsat, dexp);
        free_image(c);
        cvWaitKey(0);
    }
#endif
}


image load_image_stb(char *filename, int channels)
{
    int w, h, c;
    unsigned char *data = stbi_load(filename, &w, &h, &c, channels);
    if (!data) {
        fprintf(stderr, "Cannot load image \"%s\"\nSTB Reason: %s\n", filename, stbi_failure_reason());
        exit(0);
    }
    if(channels) c = channels;
    int i,j,k;
    image im = make_image(w, h, c);
    for(k = 0; k < c; ++k){
        for(j = 0; j < h; ++j){
            for(i = 0; i < w; ++i){
                int dst_index = i + w*j + w*h*k;
                int src_index = k + c*i + c*w*j;
                im.data[dst_index] = (float)data[src_index]/255.;
            }
        }
    }
    free(data);
    return im;
}

image load_image(char *filename, int w, int h, int c)
{
#ifdef OPENCV
    image out = load_image_cv(filename, c);
#else
    image out = load_image_stb(filename, c);
#endif

    if((h && w) && (h != out.h || w != out.w)){
        image resized = resize_image(out, w, h);
        free_image(out);
        out = resized;
    }
    return out;
}

image load_image_color(char *filename, int w, int h)
{
    return load_image(filename, w, h, 3);
}

image get_image_layer(image m, int l)
{
    image out = make_image(m.w, m.h, 1);
    int i;
    for(i = 0; i < m.h*m.w; ++i){
        out.data[i] = m.data[i+l*m.h*m.w];
    }
    return out;
}

float get_pixel(image m, int x, int y, int c)
{
    assert(x < m.w && y < m.h && c < m.c);
    return m.data[c*m.h*m.w + y*m.w + x];
}
float get_pixel_extend(image m, int x, int y, int c)
{
    if(x < 0) x = 0;
    if(x >= m.w) x = m.w-1;
    if(y < 0) y = 0;
    if(y >= m.h) y = m.h-1;
    if(c < 0 || c >= m.c) return 0;
    return get_pixel(m, x, y, c);
}
void set_pixel(image m, int x, int y, int c, float val)
{
    if (x < 0 || y < 0 || c < 0 || x >= m.w || y >= m.h || c >= m.c) return;
    assert(x < m.w && y < m.h && c < m.c);
    m.data[c*m.h*m.w + y*m.w + x] = val;
}
void add_pixel(image m, int x, int y, int c, float val)
{
    assert(x < m.w && y < m.h && c < m.c);
    m.data[c*m.h*m.w + y*m.w + x] += val;
}

void print_image(image m)
{
    int i, j, k;
    for(i =0 ; i < m.c; ++i){
        for(j =0 ; j < m.h; ++j){
            for(k = 0; k < m.w; ++k){
                printf("%.2lf, ", m.data[i*m.h*m.w + j*m.w + k]);
                if(k > 30) break;
            }
            printf("\n");
            if(j > 30) break;
        }
        printf("\n");
    }
    printf("\n");
}

image collapse_images_vert(image *ims, int n)
{
    int color = 1;
    int border = 1;
    int h,w,c;
    w = ims[0].w;
    h = (ims[0].h + border) * n - border;
    c = ims[0].c;
    if(c != 3 || !color){
        w = (w+border)*c - border;
        c = 1;
    }

    image filters = make_image(w, h, c);
    int i,j;
    for(i = 0; i < n; ++i){
        int h_offset = i*(ims[0].h+border);
        image copy = copy_image(ims[i]);
        //normalize_image(copy);
        if(c == 3 && color){
            embed_image(copy, filters, 0, h_offset);
        }
        else{
            for(j = 0; j < copy.c; ++j){
                int w_offset = j*(ims[0].w+border);
                image layer = get_image_layer(copy, j);
                embed_image(layer, filters, w_offset, h_offset);
                free_image(layer);
            }
        }
        free_image(copy);
    }
    return filters;
} 

image collapse_images_horz(image *ims, int n)
{
    int color = 1;
    int border = 1;
    int h,w,c;
    int size = ims[0].h;
    h = size;
    w = (ims[0].w + border) * n - border;
    c = ims[0].c;
    if(c != 3 || !color){
        h = (h+border)*c - border;
        c = 1;
    }

    image filters = make_image(w, h, c);
    int i,j;
    for(i = 0; i < n; ++i){
        int w_offset = i*(size+border);
        image copy = copy_image(ims[i]);
        //normalize_image(copy);
        if(c == 3 && color){
            embed_image(copy, filters, w_offset, 0);
        }
        else{
            for(j = 0; j < copy.c; ++j){
                int h_offset = j*(size+border);
                image layer = get_image_layer(copy, j);
                embed_image(layer, filters, w_offset, h_offset);
                free_image(layer);
            }
        }
        free_image(copy);
    }
    return filters;
} 

void show_image_normalized(image im, const char *name)
{
    image c = copy_image(im);
    normalize_image(c);
    show_image(c, name);
    free_image(c);
}

void show_images(image *ims, int n, char *window)
{
    image m = collapse_images_vert(ims, n);
    /*
       int w = 448;
       int h = ((float)m.h/m.w) * 448;
       if(h > 896){
       h = 896;
       w = ((float)m.w/m.h) * 896;
       }
       image sized = resize_image(m, w, h);
     */
    normalize_image(m);
    save_image(m, window);
    show_image(m, window);
    free_image(m);
}

void free_image(image m)
{
    if(m.data){
        free(m.data);
    }
}
