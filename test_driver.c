//LICENSE		"GPL3"
//AUTHOR		"Roberto de la Cruz, Juan Samper"
//DESCRIPTION	"Programa en espacio de usuario que realiza escrituras y lecturas de /dev/gpio"

#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>

//#define BUFFER_LENGTH 256               ///< The buffer length (crude but fine)
//static char receive[BUFFER_LENGTH];     ///< The receive buffer from the LKM


int thermomether(){
   FILE *ft;
   system("modprobe w1-gpio");
   system("modprobe w1-therm");

   char *device_file = "/sys/bus/w1/devices/28-0000072ae295/w1_slave";

   ft = fopen(device_file,"r");

   if (ft == NULL){
      perror("Failed to open the thermometer device");
      return errno;
   } else {
      printf("File thermometer  opened correctly!!\n");
   }

   //fgets(cadena, 100, f);
   char cadena[100]; /* Un array lo suficientemente grande como para guardar la línea mas larga del fichero */

   while (fgets(cadena, 100, ft) != NULL){
      fputs(cadena, ft);
   }

   char temperatura[3];
   int i=0;
   for(i; i<99; i++){
     if ((cadena[i] == 't') && (cadena[i+1] == '=')){
     temperatura[0]= cadena[i+2];
     temperatura[1]= cadena[i+3];
     temperatura[2] = '\0';
    }
   }
   int t;
  sscanf(temperatura, "%d", &t);

  return t;
}



int main(){
   //Obtenemos la temperatuta del ambiente a través del sensor DS18B20
   int temperatura = thermomether();

   int ret, fd, input;
   size_t size_temp = 4;
   char valor_deseado[4], valor_inicial[4];
   int length_receive = strlen(valor_deseado)/2;
   char receive[length_receive];

   fd = open("/dev/gpio", O_RDWR);             // Open the device with read/write access
   if (fd < 0){
      perror("Failed to open the device...");
      return errno;
   } else {
      printf("File opened correctly!!\n");
   }

   printf("La temperatura actual es de %i ºC \n", temperatura);

   do{
      printf("Introduzca la temperatura que desea alcalnzar [0ºC-99ºC]: ");
      scanf("%d", &input);
   } while (99 < input || input < 0);

   //Si el valor es menor que 10, añadimos un 0 delante
   if(input < 10){
      sprintf(valor_deseado, "0%d", input);
   } else {
      sprintf(valor_deseado, "%d", input);
   }

   sprintf(valor_inicial, "%d", temperatura);
   strcat(valor_deseado, valor_inicial);

   printf("Enviando datos a la FPGA para computar...\n");
   ret =  write(fd, valor_deseado, strlen(valor_deseado));
   printf("Hemos hecho el write. Enviado: %s \n", valor_deseado);

   ret = read(fd, receive, strlen(valor_deseado)/2);
   printf("Hemos hecho el read. Recibido: %s \n", receive);

   close(fd);

   printf("End of the program\n");
   return 0;
}
