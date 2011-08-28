
/******************************************************
 * wp/wpmaint.c                                       *
 * FPAC project.            FPAC PAD                  *
 *                                                    *
 * Parts of code from different sources of ax25-utils *
 *                                                    *
 * F6FBB 05-1997                                      *
 *                                                    *
 ******************************************************/

/******************************************************
 * Nov/15/06 2.01 deletes and actually erases old records
 * 12/05/97 1.00 F6FBB First draft !
 *
 ******************************************************/
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <ctype.h>

#include <time.h>
/*#include <sys/types.h>*/
/*#include <sys/fcntl.h>*/
#include <sys/file.h>
/*#include <sys/stat.h>*/
#include <sys/ioctl.h>
/*#include <sys/socket.h>*/

#include "../pathnames.h"
#include "ax25compat.h"
#include "wp.h"

int cr = 0;

/*** Prototypes *******************/
static char *my_date(time_t date);

#define CR() printf( (cr) ? "\r" : "\n"); 

int main(int ac, char **av)
{
	int i;
	int ok = 0;
	int nb = 0;
	int retour = 0;
	FILE *fptr_i;
	FILE *fptr_o;
	wp_t wp;
	wp_header wph;
	wp_header wph_sig;
	char *full_call;
	char fpacwp_old[1024];
	char *add;
	char dnic[5];
	time_t temps = time(NULL);
	time_t delete_temps = time(NULL) - 3600L * 24L * 180L;
	time_t erase_temps   = time(NULL) - 3600L * 24L * 7L;

	strcpy(fpacwp_old, FPACWP);
	strcat(fpacwp_old, ".old");
	
	if (rename(FPACWP, fpacwp_old) != 0)
	{
		fprintf(stderr, "Could not rename %s to %s ... Exiting !\n", FPACWP, fpacwp_old);
		return(1);
	}
	
	fptr_i = fopen(fpacwp_old, "r");
	if (fptr_i == NULL)
	{
		fprintf(stderr, "Could not create %s ... Exiting !\n", FPACWP);
		return(2);
	}

	fptr_o = fopen(FPACWP, "w");
	if (fptr_o == NULL)
	{
		fprintf(stderr, "Could not find %s ... Exiting !\n", fpacwp_old);
		fclose(fptr_i);
		return(2);
	}

	if (fread(&wph_sig, sizeof(wph), 1, fptr_i) == 0)
	{
		fprintf(stderr, "No signature found in %s ... Exiting\n", fpacwp_old);
		fclose(fptr_i);
		fclose(fptr_o);
		return(3);
	}

	/* Check the first record for compatibility */
	if (strcmp(wph_sig.signature,FILE_SIGNATURE) != 0)
	{
		fprintf(stderr, "WP file is not compatible\n");
		fclose(fptr_i);
		fclose(fptr_o);
		return(4);
	}

	printf("%d records in old WP database\n", wph_sig.nb_record);
	printf("user records older than 180 days are marked 'deleted'\n");
	printf("user records marked deleted are erased after 7 days\n");

	if (fwrite(&wph_sig, sizeof(wph), 1, fptr_o) == 0)
	{
		fprintf(stderr, "Cannot write signature in %s ... Exiting\n", FPACWP);
		retour = 3;
	}

	while (fread(&wp, sizeof(wp_t), 1, fptr_i))
	{
		add = rose_ntoa(&wp.address.srose_addr);

		full_call = ax25_ntoa(&wp.address.srose_call);
		if (*full_call == '\0')
			continue;

		if (wp_check_call(full_call) != 0)
		{
			printf("Illegal callsign %s : discarded\n", full_call);
			continue;
		}

		 strncpy(dnic, add, 4); dnic[4] = '\0';

		if (wp.is_node < 0 || wp.is_node > 1)
		{
			printf("Illegal is_node %d : discarded\n", wp.is_node);
			continue;
		}
		
/* User records marked deleted and older than 8 days are erased i.e. not copied */
		if (!wp.is_node && wp.is_deleted && wp.date < erase_temps) {
			printf("%-9s %s => %s %-7s", full_call, my_date(wp.date) ,dnic, add+4);
			printf("%s", " user  deleted  ERASED");
			printf("\n");
			continue;
		}
/* User records older than 180 days are marked DELETED - Nodes are NEVER deleted */ 	
		if (!wp.is_node && wp.date < delete_temps) {
			wp.is_deleted = 1;
			printf("%-9s %s => %s %-7s", full_call, my_date(wp.date) ,dnic, add+4);
			printf("%s", " user  deleted ");
			printf("\n");
			wp.date = temps;
		}
/* Node records are NEVER deleted */ 	
		if (wp.is_node) {
			wp.is_deleted = 0;
			wp.date = temps;
		}

		printf("%-9s %s => %s %-7s", full_call, my_date(wp.date), dnic, add+4);
		if (wp.is_node == 0)
		       printf("%s"," user ");
		else
			printf("%s"," node ");
		if (wp.is_deleted == 1)
			printf("%s"," deleted ");
		
		ok = 1;
		for (i = 0 ; i < wp.address.srose_ndigis ; i++)
		{
			full_call = ax25_ntoa(&wp.address.srose_digis[i]);
			if (wp_check_call(full_call) != 0)
			{
				printf("Illegal digi %s : discarded\n", full_call);
				ok = 0;
				break;
			}
		
/*			printf(" %s", full_call); */
		}
		
		if (!ok)
		{
			printf("\n");
			continue;
		}
					
		printf("\n");

		if (fwrite(&wp, sizeof(wp_t), 1, fptr_o) == 0)
		{
			fprintf(stderr, "Cannot wp record in %s ... Exiting\n", FPACWP);
			retour = 3;
		}
		
		++nb;
		
	}

/*	printf("%d records in old database\n", wph_sig.nb_record); */
	
	if (nb != wph_sig.nb_record)
	{
		wph_sig.nb_record = nb;
		rewind(fptr_o);
		
		if (fwrite(&wph_sig, sizeof(wph), 1, fptr_o) == 0)
		{
			fprintf(stderr, "Cannot write signature in %s ... Exiting\n", FPACWP);
			retour = 3;
		}


	}

	printf("%d records in new WP database\n", wph_sig.nb_record);

	fclose(fptr_i);
	fclose(fptr_o);
	return(retour);
}

static char *my_date(time_t date)
{
	static char buf[20];
	struct tm *sdate;

	sdate = localtime (&date);
	sprintf(buf, "%02d/%02d/%02d %02d:%02d", 
		sdate->tm_mday,
		sdate->tm_mon + 1, 
		sdate->tm_year%100,
		sdate->tm_hour,
		sdate->tm_min);
	return(buf);
}
