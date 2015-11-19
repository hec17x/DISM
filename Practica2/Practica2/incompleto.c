#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <Windows.h>   //Para LARGE_INTEGER
#include "imagenBMP.h"

 
// Retorna (a - b) en segundos
double performancecounter_diff(LARGE_INTEGER *a, LARGE_INTEGER *b)
{
  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  return (double)(a->QuadPart - b->QuadPart) / (double)freq.QuadPart;
}



void equalizar(ImagenBMP *img) {

    int p, vp, m = 1;
    int max = 0, min = 255;
        
    for(p = 0; p < img->tamanyo; m++ ) {                
        vp = img->datos[p];   
        if ( vp < min ) 
            min = vp;	//Hallar minimo valor de gris
        else
        if ( vp > max )
            max = vp;	//Hallar maximo valor de gris
        
        //Si hemos llegado al final de la fila,
        //saltamos al siguiente byte de datos
        if ( m % img->ancho == 0 ) 
            p += img->padding + 1;	//Los bits de padding son "de relleno"
        else
            p++;        
    }
    
    //Hacemos la ecualizacion
    //Aqui no tenemos en cuenta los bytes de padding ya que no importa
    //el valor que contengan, asi que los modificaremos tambien
    for(p = 0; p < img->tamanyo; p++)
        img->datos[p] = 255 * (img->datos[p] - min) / (max - min);

}


void equalizarMMX(ImagenBMP *img) {

    int p, vp, m = 1, j;
    int max = 0, min = 255;
	int ctecorreccion;
	short vctecorreccion[4];	// 16 x [4] bits
	unsigned char vmin[8];		//  8 x [8] bits
	double *puntero;
        

    for(p = 0; p < img->tamanyo; m++ ) {                
        vp = img->datos[p];   
        if ( vp < min ) 
            min = vp;
        else
        if ( vp > max )
            max = vp;
        
        //Si hemos llegado al final de la fila
        //saltamos al siguiente byte de datos
        if ( m % img->ancho == 0 ) 
            p += img->padding + 1;
        else
            p++;        
    }
    
    //Hacemos la ecualizacion
    //Aqui no tenemos en cuenta los bytes de padding ya que no importa
    //el valor que contengan, asi que los modificaremos tambien
    //************* INCLUIR AQUI EL CODIGO MMX !!!!!!!!!!!!!!!!!!!!!!!  

	ctecorreccion = 255 / (max - min);
	for (j = 0; j<8; j++)
		vmin[j] = min; //8 copias de minimo
	for (j = 0; j<4; j++)
		vctecorreccion[j] = ctecorreccion; //4 copias de la ctecorreccion
	for (p = 0; p<img->tamanyo; p += 8) //De 8 en 8, ya que leemos 8 pixels cada vez
	{
		puntero = (double*)&img->datos[p];
		_asm {
			//Registros edi y esi, copia eficiente de memoria apuntada por puntero
			mov esi, puntero[0]
				mov edi, puntero[0]
				// Transferencia de datos desde esi hacia mm1
				movq mm1, [esi] // mm1 = 64 bits = 8 pixels x 8 bits
								// Procesamiento de la imagen
				movq mm0, vmin // mm0 = 64 bits = 8 copias del minimo
				movq mm3, vctecorreccion // mm3=4 copias de vctecorreccion (16 bits c/u)
				psubw mm1, mm0 // mm1 = mm1 ? min = 8 pixels ? min
				movq mm2, mm1 // mm2 = copia de mm1
				pxor mm0, mm0 // inicializar mm0 = 0
				punpcklbw mm1, mm0 // mm1(0,2,4,6)=mm1 (0,1,2,3),
								   // mm1(1,3,5,7)=mm0(0,1,2,3) (bytes)
				punpckhbw mm2, mm0 // mm2(0,2,4,6)=mm2 (4,5,6,7),
								   // mm2(1,3,5,7)=mm0(4,5,6,7) (bytes)
				pmullw mm1, mm3 // multiplicar mm3*mm1 (16b x 16b)
				pmullw mm2, mm3 // idem con mm2
				packuswb mm1, mm2 // mm1(0,1,2,3)b = mm1(0?1, 2?3, 4?5, 6?7)w,
								  // mm1(4,5,6,7)=mm2(0?1, 2?3, 4?5, 6?7)
								  // Transferencia de datos a edi
				movq[edi], mm1 //puntero[0] = copia de mm1 (almacenar imagen)
		}
	}// fin for
	_asm
	{
		emms //Finalizar utilización de registros MMX
	}


}


int main(int argc, char **argv) {

    int i=0;
    struct stat buf;  
	char *dSalida;
	ImagenBMP img;
	double ganancia;
	double t_alto_nivel;
	double t_mmx;
	LARGE_INTEGER t_ini, t_fin;		//Para contar tiempo de proceso
    double secs;

    if (argc < 2) {
        fprintf(stderr,"Uso incorrecto de los parametros.\n");
        exit(1);
    }
    
    dSalida = (unsigned char*)calloc( 8 + strlen(argv[1]), 1);
    sprintf(dSalida, "salida_%s", argv[1]);    
    
    if (stat(argv[1], &buf) == 0) {
        
            fprintf(stderr, "Procesando imagen: %s ... ", argv[1]);
            leerBMP(&img, argv[1]);            
            
			//-------------------- ALTO NIVEL ----------------------------
			QueryPerformanceCounter(&t_ini);
			for (i=0; i<100; i++) //100 pruebas para poder obtener tiempos medibles
				equalizar(&img);
			QueryPerformanceCounter(&t_fin);
			secs = performancecounter_diff(&t_fin, &t_ini);
			t_alto_nivel = secs;
            fprintf(stderr, "FIN. CORRECTO. TIEMPO = %f\n",secs);
            escribirBMP(&img, dSalida);
            
			//-------------------- MMX ----------------------------
			
			QueryPerformanceCounter(&t_ini);
			for (i=0; i<100; i++) //100 pruebas para poder obtener tiempos medibles
				equalizarMMX(&img);
			QueryPerformanceCounter(&t_fin);
			secs = performancecounter_diff(&t_fin, &t_ini);
			t_mmx = secs;
            fprintf(stderr, "FIN MMX. CORRECTO. TIEMPO = %f\n",secs);
            sprintf(dSalida, "salidammx_%s", argv[1]);    
			escribirBMP(&img, dSalida);
			ganancia = t_alto_nivel / t_mmx;
			fprintf(stderr, "Ganancia respecto alto nivel= %f\n", ganancia);
		
            exit(0);
    }
    else {
        fprintf(stderr, "No existe el fichero o directorio indicado\n");
        exit(1);        
    }
    
        
    return 0;
}