#include <stdio.h>
#include "mpi.h"
#include <gmp.h>
#include <math.h>

/*
 * multiple_of_n function determines whether the given value is an integer multiple of the comparison value
 *
 * param val	int value to check if it is an integer multiple of the comparison value
 * param n	int comparison value
 * return int	representing whether val is an integer multiple of n
 */
int multiple_of_n(long val, int n){
    long i;
    for(i = 1; i < n*n; i ++){
        if (val == i*n){
            //printf("val %d is a multiple of n %d, skip\n", val, n);
            return 1;
        }
    }
    return 0;
}

/*
 * val_in_list function determines whether the given value exists in the given list
 *
 * param list		list to check
 * param list_size	size of list
 * param val		value to check list for
 * return int		representing whether the given value exists in the given list
 */
int val_in_list(long* list, int list_size, long val){
    int i;
    for (i = 0; i < list_size; i ++){
        if (list[i] == val){
            //printf("val %d is in list already, skip\n", val);
            return 1;
        }
    }
    return 0;
}

/*
 * print_arr function prints the given list
 *
 * param list		list to print values from
 * param list_size	size of list
 */
void print_arr(long* list, int list_size){
    printf("[");
    int i;
    for (i = 0; i < list_size; i ++){
        printf("%ld", list[i]);
        if (i < list_size - 1){
            printf(", ");
        }
    }
    printf("]\n");
}

main(int argc, char** argv){
    int my_rank;   /* My process rank           */
    int p;         /* The number of processes   */
    int source;    /* Process sending integral  */
    int dest = 0;  /* All messages go to 0      */
    // tags
    int ARSIZE = 0;
    int ARR = 1;
    
    MPI_Status  status;
    double elapsed_time;
    
    int n = 10;
    int startval, endval;
    
    
    //START PARELLEL CODE
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    /* Find out how many processes are being used */
    MPI_Comm_size(MPI_COMM_WORLD, &p);
    
    int N = (n*(n + 1))/2; // number of values to consider (top diagonal including diagonal). to remove top row and right col, do -2n-1
    int vals_per_p = N/p; // only add 1 if result is not an integer
    if (N % p != 0){ // result is a decimal
        vals_per_p += 1;
        //printf("added 1 to vals_per_p %d because was decimal\n", vals_per_p);
    }
    
    startval = my_rank*vals_per_p+1;
    endval = (my_rank+1)*vals_per_p;
    
    //this might be useless
    if (endval > N){
        endval -= 1;
    }
    
    printf("Proc: %d Startval: %d Endval: %d\n", my_rank, startval, endval);

    int start_col, start_row;
    int i, count = 0;
    for (i = n; i > 0; i --){
	count += i;
	if (startval <= count){
	    start_col = n - i + 1;
	    start_row = n - (count - startval);
	    break;
	}
    }


    long unique_vals[vals_per_p];
    int length_unique_vals = 0; // keeps track of how many values are in the array "unique_vals"
    
    //printf("column: %d, row: %d\n", start_col, start_row);
    
    //Set timer
    MPI_Barrier(MPI_COMM_WORLD);
    elapsed_time = -MPI_Wtime();
    
    int j;
    count = 0;
    for (i = start_col; i <= n; i ++){ // cols
        for (j = start_row; j <= n; j ++){ // rows
            long product = i*j; // store product of i*j so it doesn't need to be calculated multiple times
            
            printf("val %d * %d = %ld\n", i, j, product);
            
            if (i*j > n && !multiple_of_n(product, n) && !val_in_list(unique_vals, length_unique_vals, product)){
                unique_vals[length_unique_vals] = product;
                printf("storing unique val %d * %d = %ld at [%d]\n", i, j, product, length_unique_vals);
                length_unique_vals ++;
            }
            
            count ++;
            if (count >= vals_per_p){
                break;
            }
        }
        start_row = i + 1;
        if (count >= vals_per_p){
            break;
        }
    }
    
    printf("printing array:\n");
    print_arr(unique_vals, length_unique_vals);
    
    // communicate lists to process 0
    if (my_rank == 0) {
        long all_data[p*vals_per_p];
        int length_all_data = 0;
        
        
        // put processor 0 data into all_data
        int i;
        for(i = 0; i < length_unique_vals; i ++){
            all_data[length_all_data] = unique_vals[i];
            length_all_data ++;
        }
        
        
        for (source = 1; source < p; source++){
	    int data_size;
	    MPI_Recv(&data_size, 1, MPI_LONG, source, ARSIZE, MPI_COMM_WORLD, &status); // get size of data array
				
	    long data[data_size];
            MPI_Recv(&data, data_size, MPI_LONG, source, ARR, MPI_COMM_WORLD, &status); // get data array
            
            //printf("received: ");
            //print_arr(data, data_size);
            
            int new_vals = 0; // counter for how many new values are added to all_data (to update length_all_data)
            int i, j;
            for (i = 0; i < data_size; i ++){
                int is_unique = 1;
                
                for(j = 0; j < length_all_data; j ++){
                    if (all_data[j] == data[i]){ // means that we have checked all values of data already. -1 is a place holder
                        is_unique = 0;
                        break;
                    }
                }
                if (is_unique == 1){
		    //printf("Adding %d to all_data\n", data[i]);
                    all_data[length_all_data + new_vals] = data[i];
                    new_vals ++;
                }
            }
            length_all_data += new_vals;
        }
        
        
        for (i = 1; i <= n; i ++){
            // append 1 to n and integer multiples of n to all_data
            all_data[length_all_data] = i;
            length_all_data ++;
            if (i != 1){
                all_data[length_all_data] = i*n;
                length_all_data ++;
            }
        }
        
        //Update timer
        elapsed_time += MPI_Wtime();
        printf("Final values: ");
        print_arr(all_data, length_all_data);
        printf("total unique numbers: %d\n", length_all_data);
        printf("Elasped Time: %f\n",elapsed_time);
    }else{
        
        //send size of unique_vals array
        MPI_Send(&length_unique_vals, 1, MPI_LONG, dest, ARSIZE, MPI_COMM_WORLD);
        //send unique_vals array
        MPI_Send(&unique_vals, length_unique_vals, MPI_LONG, dest, ARR, MPI_COMM_WORLD);
        
        //printf("sending: ");
        //print_arr(unique_vals, length_unique_vals);
    }
    MPI_Finalize();
}
