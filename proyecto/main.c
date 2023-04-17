#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Dimensiones de la ventana
float ANCHO = 1000;
float ALTO = 1000;

// Angulo actual de la rotacion
float rotation_angle = 0.0;

// Bodalidades
bool borde = true;
bool texture = false;
bool relleno = false;
// Textura ID
GLuint textureID;

// Valores minimos y maximos en coordenadas universales
float xMin = -86.5;
float xMax = -82;
float yMin = 7;
float yMax = 11.5;

// Valores minimos y maximos en coordenadas de framebuffer
float xMin2 = 0;
float xMax2 = 800;
float yMin2 = 0;
float yMax2 = 800;

// Dimensiones de imagen de la textura
float xMin3 = 0;
float xMax3 = 870;
float yMin3 = 0;
float yMax3 = 390;

// Estructura para almacenar las coordenadas de un vértice
typedef struct
{
   float x;
   float y;
} Vertice;

// Estructura para almacenar los datos de un polígono
typedef struct
{
   Vertice *vertices;
   int num_vertices;
   int color[3];
} Poligono;

int compare(const void *a, const void *b)
{
   GLfloat *x1 = (GLfloat *)a;
   GLfloat *x2 = (GLfloat *)b;
   if (*x1 < *x2)
   {
      return -1;
   }
   else if (*x1 > *x2)
   {
      return 1;
   }
   else
   {
      return 0;
   }
}

// Funcion para mapear a coordenadas de framebuffer
void map_to_framebuffer(float universalesX, float universalesY, int width, int height, int *x_fb, int *y_fb)
{
   // Regla de 3 para mapear a coordenadas de framebuffer
   *x_fb = (int)((universalesX - xMin) / (xMax - xMin) * width);  // para x
   *y_fb = (int)((universalesY - yMin) / (yMax - yMin) * height); // para y
}

// Función para cargar los datos de un archivo de polígonos
Poligono cargar_poligono(char *archivo)
{

   FILE *f = fopen(archivo, "r");
   if (f == NULL)
   {
      printf("No se pudo abrir el archivo %s\n", archivo);
      exit(1);
   }

   Poligono poligono;
   poligono.num_vertices = 0;

   // Contar el número de vértices del polígono
   char linea[100];
   while (fgets(linea, 100, f) != NULL)
   {
      poligono.num_vertices++;
   }

   // Reservar memoria para los vértices del polígono
   poligono.vertices = (Vertice *)malloc(poligono.num_vertices * sizeof(Vertice));

   // Regresar al principio del archivo
   rewind(f);

   // Leer las coordenadas de los vértices del polígono
   int i = 0;
   while (fgets(linea, 100, f) != NULL)
   {
      sscanf(linea, "%f %f", &poligono.vertices[i].x, &poligono.vertices[i].y); // guarda los datos en el poligono(x,y)
      // Empezando mapeo a una ventana de 800*800
      int x_fb, y_fb;
      map_to_framebuffer(poligono.vertices[i].x, poligono.vertices[i].y, ANCHO, ALTO, &x_fb, &y_fb); // funcion para mapear
      poligono.vertices[i].x = x_fb;                                                                 // x ya mapeado
      poligono.vertices[i].y = y_fb;                                                                 // y ya mapeado
      i++;
   }

   fclose(f);

   return poligono;
}

// Funcion para cargar una imagen para la textura (.BMP)
GLuint loadBMPTexture(const char *filename)
{
   FILE *file = fopen(filename, "rb");
   if (!file)
   {
      fprintf(stderr, "Error al abrir el archivo: %s\n", filename);
      exit(EXIT_FAILURE);
   }

   unsigned char header[54];
   fread(header, sizeof(unsigned char), 54, file);

   // Tamaño de la imagen de la textura
   int width = *(int *)&header[18];
   int height = *(int *)&header[22];

   int imageSize = width * height * 3;
   unsigned char *imageData = malloc(imageSize * sizeof(unsigned char));
   fread(imageData, sizeof(unsigned char), imageSize, file);
   fclose(file);

   glGenTextures(1, &textureID);
   glBindTexture(GL_TEXTURE_2D, textureID);
   // Configurar textura
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // GL_CLAMP_TO_EDGE
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, width, height, 0x80E0, GL_UNSIGNED_BYTE, imageData); // BGR = 0x80E0
   free(imageData);
   return textureID;
}

void draw_line_bresenham(int x0, int y0, int x1, int y1, int color[3])
{
   int dx = abs(x1 - x0);
   int dy = abs(y1 - y0);
   int sx = (x0 < x1) ? 1 : -1;
   int sy = (y0 < y1) ? 1 : -1;
   int err = dx - dy;

   while (1)
   {
      glColor3ub(color[0], color[1], color[2]);
      glVertex2i(x0, y0);

      if (x0 == x1 && y0 == y1)
         break;

      int e2 = 2 * err;
      if (e2 > -dy)
      {
         err -= dy;
         x0 += sx;
      }
      if (e2 < dx)
      {
         err += dx;
         y0 += sy;
      }
   }
}

// Algorimot scanline v2 para poligonos irregulares(Cóncavos)
void scanlineFill(Poligono poligono)
{
   int minY = INT_MAX;
   int maxY = INT_MIN;
   int scanline;
   // Si textura esta activado
   if (texture)
   {
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, textureID);
   }
   glColor3ub(poligono.color[0], poligono.color[1], poligono.color[2]); // por default el valor
   // Encuentra los valores mínimos y máximos de y entre los vértices del polígono
   for (int i = 0; i < poligono.num_vertices; i++)
   {
      if (poligono.vertices[i].y < minY)
      {
         minY = poligono.vertices[i].y;
      }
      if (poligono.vertices[i].y > maxY)
      {
         maxY = poligono.vertices[i].y;
      }
   }

   scanline = maxY;

   while (scanline >= minY)
   {
      // Calcular intersecciones con bordes activos
      GLfloat intersections[poligono.num_vertices];
      int numIntersections = 0;

      for (int i = 0; i < poligono.num_vertices; i++)
      {
         GLfloat x1 = poligono.vertices[i].x;
         GLfloat y1 = poligono.vertices[i].y;
         GLfloat x2 = poligono.vertices[(i + 1) % poligono.num_vertices].x;
         GLfloat y2 = poligono.vertices[(i + 1) % poligono.num_vertices].y;

         if (y1 == y2)
            continue; // Ignora las líneas horizontales

         if (y1 > y2)
         {
            GLfloat tempX = x1;
            GLfloat tempY = y1;
            x1 = x2;
            y1 = y2;
            x2 = tempX;
            y2 = tempY;
         }

         if (scanline > y1 && scanline <= y2)
         {
            GLfloat xIntersection = x1 + (scanline - y1) * (x2 - x1) / (y2 - y1);
            intersections[numIntersections++] = xIntersection;
         }
      }

      // Ordenar las intersecciones
      qsort(intersections, numIntersections, sizeof(GLfloat), compare);
      //  Pintar de 2 en 2
      for (int i = 0; i < numIntersections - 1; i += 2)
      {
         for (int x = intersections[i]; x <= intersections[i + 1]; x++)
         {
            // Textura
            if (texture)
            {
               if (!relleno)
                  glColor3ub(255, 255, 255);
               // Regla de 3 para mapear de coordenadas de la imagen a framebuffer
               float u = (x - xMin3) / (xMax3 - xMin3);
               float v = (scanline - yMin3) / (yMax3 - yMin3);
               glTexCoord2f(u, v); // pintar textura
            }
            // Rellena el poligono
            glBegin(GL_POINTS);
            glVertex2i(x, scanline);
            glEnd();
         }
      }

      scanline--;
   }

   glDisable(GL_TEXTURE_2D);
}

// Función para dibujar un polígono
void dibujar_poligono(Poligono poligono, int color2[])
{
   // Se guarda el color en el poligono
   poligono.color[0] = color2[0];
   poligono.color[1] = color2[1];
   poligono.color[2] = color2[2];

   // Si borde es true se dibujan los bordes
   if (borde)
   {
      glBegin(GL_POINTS);
      for (int i = 0; i < poligono.num_vertices - 1; i++)
      {
         draw_line_bresenham(
             poligono.vertices[i].x,
             poligono.vertices[i].y,
             poligono.vertices[(i + 1)].x,
             poligono.vertices[(i + 1)].y,
             poligono.color);
      }
      glEnd();
   }
   // Rellena el polígono utilizando el algoritmo de Scanline
   if (relleno || texture)
      scanlineFill(poligono);
}

// zoom
void zoom(char opc, float factor)
{
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

   switch (opc)
   {
   case '+':
      xMin2 += 5;
      xMax2 -= 5;
      yMin2 += 5;
      yMax2 -= 5;
      break;
   case '-':
      xMin2 -= 5;
      xMax2 += 5;
      yMin2 -= 5;
      yMax2 += 5;
      break;
   case 'r': // reset
      xMin2 = 0;
      xMax2 = ANCHO;
      yMin2 = 0;
      yMax2 = ALTO;
   }
   gluOrtho2D(xMin2, xMax2, yMin2, yMax2); // Define la vista de la ventana de visualización
   glMatrixMode(GL_MODELVIEW);
}

// Pan
// la ventana establecida podra moverse hacia la izquierda,derecha, arriba o abajo.
void pan(char opc, float y_offset)
{
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   switch (opc)
   {
   case 'w':
      yMin2 += 5;
      yMax2 += 5;
      break;
   case 's':
      yMin2 -= 5;
      yMax2 -= 5;
      break;
   case 'd':
      xMin2 += 5;
      xMax2 += 5;
      break;
   case 'a':
      xMin2 -= 5;
      xMax2 -= 5;
      break;
   case 'r': // reset
      xMin2 = 0;
      xMax2 = ANCHO;
      yMin2 = 0;
      yMax2 = ALTO;
   }
   gluOrtho2D(xMin2, xMax2, yMin2, yMax2); // Define la vista de la ventana de visualización
   glMatrixMode(GL_MODELVIEW);
}

// rotate
void rotate(float angle)
{
   rotation_angle += angle; // incrementar el ángulo en cada iteración
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glRotatef(rotation_angle, 0, 0, 1); // funcion para rotar el poligono
   gluOrtho2D(0, ANCHO, 0, ALTO);
   glMatrixMode(GL_MODELVIEW);
}
// Función para dibujar el mapa de Costa Rica
void dibujar_mapa()
{
   // Color para cada provincia (RGB)
   int color[7][3] = {{255, 0, 255},   // 0
                      {0, 204, 0},     // 1
                      {127, 0, 255},   // 2
                      {51, 0, 0},      // 3
                      {240, 230, 140}, // 4
                      {135, 206, 235}, // 5
                      {0, 153, 153}};  // 6
   // Dibujar provincias
   textureID = loadBMPTexture("2.bmp");
   xMax3 = 551;
   yMax3 = 274;
   dibujar_poligono(cargar_poligono("puntarenas.txt"), color[4]);
   dibujar_poligono(cargar_poligono("puntarenas0.txt"), color[4]);
   dibujar_poligono(cargar_poligono("puntarenas1.txt"), color[4]);
   dibujar_poligono(cargar_poligono("puntarenas2.txt"), color[4]);
   dibujar_poligono(cargar_poligono("puntarenas3.txt"), color[4]);
   textureID = loadBMPTexture("2.bmp");
   xMax3 = 551;
   yMax3 = 274;
   dibujar_poligono(cargar_poligono("sanjose.txt"), color[0]);
   textureID = loadBMPTexture("4.bmp");
   xMax3 = 573;
   yMax3 = 219;
   dibujar_poligono(cargar_poligono("alajuela.txt"), color[1]);
   dibujar_poligono(cargar_poligono("heredia.txt"), color[2]);
   textureID = loadBMPTexture("1.bmp");
   xMax3 = 870;
   yMax3 = 390;
   dibujar_poligono(cargar_poligono("cartago.txt"), color[3]);
   textureID = loadBMPTexture("1.bmp");
   xMax3 = 870;
   yMax3 = 390;
   dibujar_poligono(cargar_poligono("guanacaste.txt"), color[5]);
   textureID = loadBMPTexture("3.bmp");
   xMax3 = 618;
   yMax3 = 214;
   dibujar_poligono(cargar_poligono("limon.txt"), color[6]);
}

// Función para dibujar la escena
void display()
{
   glClear(GL_COLOR_BUFFER_BIT); // bora todo lo dibujado
   dibujar_mapa();
   glFlush();
}

void keyboard(unsigned char key, int x, int y)
{
   switch (key)
   {
   case '1':
      // Cambiar el modo de visualización a coloreado
      borde = true;
      relleno = false;
      texture = false;
      display();
      break;
   case '2':
      // Cambiar el modo de visualización a coloreado
      relleno = true;
      texture = false;
      borde = false;
      display();
      break;
   case '3':
      // Cambiar el modo de visualización a texturas
      texture = true;
      relleno = false;
      borde = false;
      display();
      break;
   case '4':
      // Cambiar el modo de visualización a texturas
      relleno = true;
      texture = true;
      display();
      break;
   case '+':
      // Zoom in
      zoom('+', 0.01);
      rotation_angle = 0;
      break;
   case '-':
      // Zoom out
      zoom('-', 0.01);
      rotation_angle = 0;
      break;
   case 'w':
      // Pan Up
      pan('w', 10);
      break;
   case 's':
      // Pan Down
      pan('s', -10);
      break;
   case 'a':
      // Pan Left
      pan('a', 0);
      break;
   case 'd':
      // Pan Right
      pan('d', 0);
      break;
   case 'e':
      // Rotación en sentido horario
      rotate(-10);
      break;
   case 'q':
      // Rotación en sentido antihorario
      rotate(10);
      break;
   case 'r':
      // Reiniciar
      // Se restablece la vista y las transformaciones a su estado inicial
      zoom('r', 0.01);
      rotation_angle = 0;
      break;
   case ' ': // Espacio termina el programa
   case 27:
      // Terminar
      exit(0);
      break;
   }
   glutPostRedisplay(); // actualiza la ventana
}

// Redibuja cada que se mueve la ventana
void reshape(int w, int h)
{
   ANCHO = w; // width
   ALTO = h;  // high
   xMin2 = 0;
   xMax2 = w;
   yMin2 = 0;
   yMax2 = h;
   glViewport(0, 0, w, h);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluOrtho2D(0, w, 0, h); // Define la vista de la ventana de visualización
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
}
void initGL()
{
   glClearColor(0, 0, 0, 1);
   gluOrtho2D(0, ANCHO, 0, ALTO);
}

void mouseWheel(int btn, int state, int x, int y)
{
   if (state == GLUT_DOWN)
   {
      switch (btn)
      {
      case 3: // mouse wheel scroll up
         zoom('+', 0.01);
         rotation_angle = 0;
         break;
      case 4: // mouse wheel scroll down
         zoom('-', 0.01);
         rotation_angle = 0;
         break;
      default:
         break;
      }
   }
   glutPostRedisplay();
}
int main(int argc, char **argv)
{
   glutInit(&argc, argv);
   glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
   glutInitWindowSize(ANCHO, ALTO);
   glutCreateWindow("Mapa de Costa Rica");
   initGL();
   glutDisplayFunc(display);   // hace el trabajo
   glutKeyboardFunc(keyboard); // funcion para para el teclado
   glutMouseFunc(mouseWheel);  // funcion para el mouse
   glutReshapeFunc(reshape);   // actualiza la ventana
   glutMainLoop();
   return 0;
}