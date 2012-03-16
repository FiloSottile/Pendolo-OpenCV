/*
 * Misurazione della durata delle oscillazioni di un pendolo
 * Scritto con le librerie OpenCV
 * Compilare con: g++ -Wall -I/usr/include/opencv  -lml -lcvaux -lhighgui -lcv -lcxcore
 * Eseguire con un video compatibile come unico parametro
 * Crea il video "out.avi" e il csv "log.csv"
 * Filippo Valsorda - Dic 2010 - filosottile.wiki gmail.com
 * http://www.pytux.it
 */

#include <cvaux.h>
#include <highgui.h>
#include <cxcore.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>
#include <sys/time.h>

#include <iostream>
#include <fstream>

using namespace std;

int main(int argc, char* argv[])
{
    // Millisecondi all'inizio del processo -> ts
    timeval start;
    gettimeofday(&start, NULL);
    double ts = start.tv_sec*1000+(start.tv_usec/1000.0);
    
    // Controllo l'input
    CvCapture* capture = cvCaptureFromAVI(argv[1]);
    if( !capture ) {
        fprintf( stderr, "ERROR: capture is NULL \n" );
        //getchar();
        exit(-1);
    }
    
    if ( !cvGrabFrame(capture) ) {
        fprintf( stderr, "ERROR: capture is EMPTY \n" );
        //getchar();
        exit(-1);
    }
    
    // Ottengo SIZE
    int frameH = (int) cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT);
    int frameW = (int) cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH);
    CvSize SIZE = cvSize(frameW, frameH);
    
    // Apro il video di destinazione
    CvVideoWriter *writer = 0;
    int isColor = 1;
    int fps = 24;
    writer = cvCreateVideoWriter("out.avi", CV_FOURCC('D', 'I', 'V', 'X'), fps, SIZE, isColor);
    
    // Creo le finestre
    cvNamedWindow("Camera", CV_WINDOW_AUTOSIZE);
    //cvNamedWindow("HSV", CV_WINDOW_AUTOSIZE);
    cvNamedWindow("Colore filtrato", CV_WINDOW_AUTOSIZE);
    
    // Apertura log
    ofstream log;
    log.open("log.csv");
    log << "Lato" << ',' << "X (pixel)" << ',' << "Istante (ms)" << ',' << "Durata (ms)" << endl;
    
    // Info per il testo
    CvFont font;
    cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 1.0, 1.0, 0, 1, CV_AA);
    CvScalar TEXT_COLOR = cvScalar(0, 0, 0, 0);
    int MARGIN_UP = 130;
    int MARGIN_LEFT = 10;
    int TEXT_SPACING = 40;
    
    // RANGE per il color detection
    // Il programma permette il riconoscimento di due range di colori HSV
    // che verranno riconosciuti entrambi (pixel OR)
    // Consiglio l'uso della tavolozza di GIMP con lo strumento per 
    // riconoscere il colore su un fotogramma del video
    CvScalar hsv_min = cvScalar(0, 125, 125, 0);
    CvScalar hsv_max = cvScalar(12, 255, 255, 0);
    
    CvScalar hsv_min2 = cvScalar(340, 0, 0, 0);
    CvScalar hsv_max2 = cvScalar(360, 255, 255, 0);
    
    // Dichiarazioni VARIE
    IplImage* img = 0; 
    IplImage* hsv_frame = cvCreateImage(SIZE, IPL_DEPTH_8U, 3);
    IplImage* thresholded = cvCreateImage(SIZE, IPL_DEPTH_8U, 1);
    IplImage* thresholded1 = cvCreateImage(SIZE, IPL_DEPTH_8U, 1);
    IplImage* thresholded2 = cvCreateImage(SIZE, IPL_DEPTH_8U, 1);
    
    float posMsec;
    timeval tim;
    double t1;
    
    char tempo[12];
    char xy[20];
    char most[50];
    char lato_txt[20];
    
    int periodi = 0;
    
    short lato = -1; // 0 sx 1 dx
    short ultimo_lato = -1;
    double mostdx = 0.0;
    double mostdx_sec = -1;
    double mostsx = 2000.0;
    double mostsx_sec = -1;
    double ultimo_periodo = -1;
    
    do { // Primo frame già grabbed per conferma
    
        // Ottengo l'immagine
        img=cvRetrieveFrame(capture);
        
        // Ottendo i dati sul tempo
        gettimeofday(&tim, NULL);
        t1=tim.tv_sec*1000+(tim.tv_usec/1000.0);
        posMsec = cvGetCaptureProperty(capture, CV_CAP_PROP_POS_MSEC);
        //cout << posMsec << endl;
        
        // Scrivo i millisecondi
        sprintf ( tempo, "%.0f", posMsec );
        cvPutText(img, tempo, cvPoint(MARGIN_LEFT, MARGIN_UP+TEXT_SPACING*0), &font, TEXT_COLOR);
        sprintf ( tempo, "%.0f", t1-ts );
        cvPutText(img, tempo, cvPoint(MARGIN_LEFT, MARGIN_UP+TEXT_SPACING*1), &font, TEXT_COLOR);
        
        // Converto a HSV
        cvCvtColor(img, hsv_frame, CV_BGR2HSV);
        
        // Riconoscimento colore
        cvInRangeS(hsv_frame, hsv_min, hsv_max, thresholded1);
        cvInRangeS(hsv_frame, hsv_min2, hsv_max2, thresholded2);
        cvOr(thresholded1, thresholded2, thresholded);
        
        // Correzione dell'immagine
        //cvSmooth( thresholded, thresholded, CV_GAUSSIAN, 9, 9 );
        cvSmooth( thresholded, thresholded, CV_MEDIAN, 13, 0, 0, 0 );
        
        // Trovo la palla
        CvMemStorage* storage = cvCreateMemStorage(0); // Memory for hough circles
        CvSeq* circles = cvHoughCircles(thresholded, storage, CV_HOUGH_GRADIENT, 2,
                                        thresholded->width, 100, 30, 10, 400);
        
        if (circles->total == 1) {
            // Se c'è la palla
            
            // Ottengo e scrivo le coordinate
            float* p = (float*)cvGetSeqElem( circles, 0 );
            sprintf ( xy, "Ball(%.0f; %.0f)", p[0],p[1] );
            cvPutText(img, xy, cvPoint(MARGIN_LEFT, MARGIN_UP+TEXT_SPACING*2), &font, TEXT_COLOR);
            
            // Disegno cerchio e punto
            cvCircle(img, cvPoint(cvRound(p[0]),cvRound(p[1])), 3, CV_RGB(0,255,0), -1, 8, 0 );
            cvCircle(img, cvPoint(cvRound(p[0]),cvRound(p[1])), cvRound(p[2]), CV_RGB(255,0,0), 3, 8, 0 );
                                    
            if (p[0] > frameW*0.53125) { 
                // Se siamo a destra
                lato = 1;
                
                // Se stiamo all'apice dell'oscillazione
                if (p[0] > mostdx) { 
                    mostdx = p[0]; 
                    mostdx_sec = posMsec;
                }
                
                // Se abbiamo appena completato un'oscillazione a sinistra
                if (ultimo_lato == 0) {
                    cout << "SX: " << (int)mostsx << "   --   Secondi " << (int)mostsx_sec << endl;
                    cout << "DURATA " << (int)(mostsx_sec - ultimo_periodo) << endl;
                    
                    // Se non è assestamento, scivi nel log
                    if (periodi < 3) { cout << "Assestamento..." << endl; }
                    else { log << "SX" << ',' << (int)mostsx << ',' << (int)mostsx_sec << ',' \
                               << (int)(mostsx_sec - ultimo_periodo) << endl; }
                    
                    periodi++;
                    ultimo_periodo = mostsx_sec;
                    mostsx = 2000.0;
                }
                
                // Imposto l'indicatore
                ultimo_lato = 1;
                
            } else if (p[0] < frameW*0.46875) { 
                // Se siamo a sinistra
                lato = 0;
                
                // Se siamo all'apice dell'oscillazione
                if (p[0] < mostsx) {
                    mostsx = p[0];
                    mostsx_sec = posMsec;
                }
                
                // Se abbiamo appena completato un'oscillazione a destra
                if (ultimo_lato == 1) {
                    cout << "DX: " << (int)mostdx << "   --   Secondi " << (int)mostdx_sec << endl;
                    cout << "DURATA " << (int)(mostdx_sec - ultimo_periodo) << endl;
                    
                    // Se non è assestamento, scivi nel log
                    if (periodi < 3) { cout << "Assestamento..." << endl; }
                    else { log << "DX" << ',' << (int)mostdx << ',' << (int)mostdx_sec << ',' \
                               << (int)(mostdx_sec - ultimo_periodo) << endl; }
                    
                    periodi++;
                    ultimo_periodo = mostdx_sec;
                    mostdx = 0.0;
                }
                
                // Imposto l'indicatore
                ultimo_lato = 0;
                
            } else { 
                // Siamo al centro... rilassiamoci
                lato = -1; 
            }
        } else {
            // La palla non c'è
            cvPutText(img, "NO Ball", cvPoint(MARGIN_LEFT, MARGIN_UP+TEXT_SPACING*2), &font, TEXT_COLOR);
        }
        
        // Scrivo il lato e le oscillazioni
        if (lato == 0) { sprintf(lato_txt, "SX"); }
        else if (lato == 1) { sprintf(lato_txt, "DX"); }
        else { sprintf(lato_txt, "--"); }
        sprintf( tempo, "%s - %i", lato_txt, periodi );
        cvPutText(img, tempo, cvPoint(MARGIN_LEFT, MARGIN_UP+TEXT_SPACING*3), &font, TEXT_COLOR);
        // e i massimi valori delle X
        sprintf( most, "%.0f - %.0f", mostsx, mostdx );
        cvPutText(img, most, cvPoint(MARGIN_LEFT, MARGIN_UP+TEXT_SPACING*4), &font, TEXT_COLOR);
        
        // Mostro le immagini
        cvShowImage("Colore filtrato", thresholded);        
        cvShowImage("Camera", img); 
        //cvShowImage("HSV", hsv_frame);  
        
        // Salvo sul video
        cvWriteFrame(writer,img);
        
        // Tasto ESC
        if( (cvWaitKey(10) & 255) == 27 ) break;
        cvWaitKey(40);
        
    } while (cvGrabFrame(capture));
    
    // Chiudo
    log.close();
    cvReleaseVideoWriter(&writer);
}
