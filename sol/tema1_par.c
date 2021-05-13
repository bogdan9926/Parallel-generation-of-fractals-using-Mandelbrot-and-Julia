#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#define min(x,y) ((x) < (y) ?  (x) : (y))
//#define NUM_THREADS 2
char *in_filename_julia;
char *in_filename_mandelbrot;
char *out_filename_julia;
char *out_filename_mandelbrot;

int P;
int widthJ, heightJ,widthM,heightM;
int **result, **resultM;

pthread_barrier_t barrier;
pthread_mutex_t mutex;

typedef struct _complex {
	double a;
	double b;
} complex;

// structura pentru parametrii unei rulari
typedef struct _params {
	int is_julia, iterations;
	double x_min, x_max, y_min, y_max, resolution;
	complex c_julia;
} params;
params parJ;
params parM;

void get_args(int argc, char **argv)
{
	if (argc < 6) {
		printf("Numar insuficient de parametri:\n\t"
				"./tema1_par fisier_intrare_julia fisier_iesire_julia "
				"fisier_intrare_mandelbrot fisier_iesire_mandelbrot P\n");
		exit(1);
	}

	in_filename_julia = argv[1];
	out_filename_julia = argv[2];
	in_filename_mandelbrot = argv[3];
	out_filename_mandelbrot = argv[4];
    P = atoi(argv[5]);
}

void read_input_file(char *in_filename, params* par)
{
	FILE *file = fopen(in_filename, "r");
	if (file == NULL) {
		printf("Eroare la deschiderea fisierului de intrare!\n");
		exit(1);
	}

	fscanf(file, "%d", &par->is_julia);
	fscanf(file, "%lf %lf %lf %lf",
			&par->x_min, &par->x_max, &par->y_min, &par->y_max);
	fscanf(file, "%lf", &par->resolution);
	fscanf(file, "%d", &par->iterations);

	if (par->is_julia) {
		fscanf(file, "%lf %lf", &par->c_julia.a, &par->c_julia.b);
	}

	fclose(file);
}

void write_output_file(char *out_filename, int **result,int width,int height)
{
	int i, j;

	FILE *file = fopen(out_filename, "w");
	if (file == NULL) {
		printf("Eroare la deschiderea fisierului de iesire!\n");
		return;
	}
	fprintf(file, "P2\n%d %d\n255\n", width, height);
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			fprintf(file, "%d ", result[i][j]);
		}
		fprintf(file, "\n");
	}

	fclose(file);
}


int **allocate_memory(int width, int height)
{
	int **result;
	int i;

	result = malloc(height * sizeof(int*));
	if (result == NULL) {
		printf("Eroare la malloc!\n");
		exit(1);
	}

	for (i = 0; i < height; i++) {
		result[i] = malloc(width * sizeof(int));
		if (result[i] == NULL) {
			printf("Eroare la malloc!\n");
			exit(1);
		}
	}

	return result;
}

// elibereaza memoria alocata
void free_memory(int **result, int height)
{
	int i;

	for (i = 0; i < height; i++) {
		free(result[i]);
	}
	free(result);
}
void run_julia(int **result, int startW, int endW, int startH, int endH,int height, int width)
{
	//W = width, H = height, J = julia
	int w, h, i;
	//interval (startW, endW) care depinde de thread
	for (w = startW; w < endW; w++) {
		//interval (0, heightJ) care depinde de thread
		for (h = 0; h < heightJ; h++) {
			int step = 0;
			complex z = { .a = w * parJ.resolution + parJ.x_min,
							.b = h * parJ.resolution + parJ.y_min };

			while ((sqrt(pow(z.a, 2.0) + pow(z.b, 2.0)) < 2.0) && step < parJ.iterations) {
				complex z_aux = { .a = z.a, .b = z.b };

				z.a = pow(z_aux.a, 2) - pow(z_aux.b, 2) + parJ.c_julia.a;
				z.b = 2 * z_aux.a * z_aux.b + parJ.c_julia.b;

				step++;
			}

			result[h][w] = step % 256;
		}
	}
    pthread_barrier_wait(&barrier);
	// transforma rezultatul din coordonate matematice in coordonate ecran

	//interval (startH, endH) care depinde de thread
	for (i = startH; i < endH; i++) {
		int *aux = result[i];
		result[i] = result[height - i - 1];
		result[height - i - 1] = aux;
	}
}

void run_mandelbrot(int **result, int startW, int endW, int startH, int endH,int height, int width)
{
	//W = width, H = height, M= mandelbrot
	int w, h, i;
	//interval (startW, endW) care depinde de thread
	for (w = startW; w < endW; w++) {
		//interval (0, heightM) care depinde de thread
		for (h = 0; h < heightM; h++) {
			complex c = { .a = w * parM.resolution + parM.x_min,
							.b = h * parM.resolution + parM.y_min };
			complex z = { .a = 0, .b = 0 };
			int step = 0;

			while ((sqrt(pow(z.a, 2.0) + pow(z.b, 2.0)) < 2.0) && step < parM.iterations) {
				complex z_aux = { .a = z.a, .b = z.b };

				z.a = pow(z_aux.a, 2.0) - pow(z_aux.b, 2.0) + c.a;
				z.b = 2.0 * z_aux.a * z_aux.b + c.b;

				step++;
			}

			result[h][w] = step % 256;
		}
	}
    pthread_barrier_wait(&barrier);

	// transforma rezultatul din coordonate matematice in coordonate ecran
	//interval (startH, endH) care depinde de thread
	for (i = startH; i < endH; i++) {
		int *aux = result[i];
		result[i] = result[height - i - 1];
		result[height - i - 1] = aux;
	}
}


void *thread_function(void *arg)
{
    int thread_id = *(int *)arg;
	//calcularea intervalului in care actioneaza un thread
	//W=width, H=height, J=Julia, M = Mandelbrot
	
	int startWJ = thread_id *(double)widthJ/P;
	int endWJ = min((thread_id+1) *(double)widthJ/P,widthJ);

    int startHJ = thread_id *(double)heightJ/(P*2);
	int endHJ = min((thread_id+1) *(double)heightJ/(P*2),heightJ/2);

    int startWM = thread_id *(double)widthM/P;
	int endWM = min((thread_id+1) *(double)widthM/P,widthM);

    int startHM = thread_id *(double)heightM/(P*2);
	int endHM = min((thread_id+1) *(double)heightM/(P*2),heightM/2);

	//se aloca memorie pt julia de catre un singur thread
	if(thread_id==0) {
		result = allocate_memory(widthJ, heightJ);
	}
	//se asteapta dupa alocare
	pthread_barrier_wait(&barrier);

	//se apeleaza algoritmul julia
    run_julia(result,startWJ,endWJ,startHJ,endHJ,heightJ,widthJ);
	pthread_barrier_wait(&barrier);

	//se scrie rezultatul, se elibereaza memoria si se realoca memorie, 
	//de catre un singur thread
	if(thread_id == 0) {
		write_output_file(out_filename_julia, result, widthJ,heightJ);
    	free_memory(result,heightJ);
		result = allocate_memory(widthM, heightM);

	}
	//se asteapta dupa alocare
	pthread_barrier_wait(&barrier);
		
	//se apeleaza algoritmul mandelbrot
	run_mandelbrot(result,startWM,endWM,startHM,endHM,heightM,heightJ);
   	pthread_barrier_wait(&barrier);	
    
	//se scrie rezultatul si se elibereaza memoria, 
	//de catre un singur thread
	if(thread_id ==0) {
		write_output_file(out_filename_mandelbrot, result, widthM,heightM);
    	free_memory(result,heightM);
	}
		

    pthread_exit(NULL);
}






int main(int argc, char *argv[]) {

    int i;
    get_args(argc, argv);

    pthread_t tid[P];
    int thread_id[P];

   
	//bariera
    pthread_barrier_init(&barrier, NULL, P);

	//citim din cele 2 fisiere in 2 structuri diferite
    read_input_file(in_filename_julia, &parJ);
	read_input_file(in_filename_mandelbrot, &parM);

	//calculam rezolutiile pozei
	widthJ = (parJ.x_max - parJ.x_min) / parJ.resolution;
	heightJ = (parJ.y_max - parJ.y_min) / parJ.resolution;
    widthM = (parM.x_max - parM.x_min) / parM.resolution;
	heightM = (parM.y_max - parM.y_min) / parM.resolution;


 

    
	//creearea de thread-uri
    for (i = 0; i < P; i++) {
		thread_id[i] = i;
		pthread_create(&tid[i], NULL, thread_function, &thread_id[i]);
	}

	for (i = 0; i < P; i++) {   
		pthread_join(tid[i], NULL);
	}

    return 0;
}
