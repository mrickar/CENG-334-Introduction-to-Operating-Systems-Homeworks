#include "logging.h"


long get_timestamp()
{
    struct timeval cur_time;
    gettimeofday(&cur_time, NULL);
    return cur_time.tv_sec * 1000000 + cur_time.tv_usec;
}

void print_output(imp *in, omp *out, obsd *obstacle, od* objects) {
    print_output_helper(stdout, in, out, obstacle, objects);
}

void print_output_helper(FILE* file, imp *in, omp *out, obsd *obstacle, od* objects) {

    long time = get_timestamp();
    if ( in ) {
        fprintf(file, "0 %ld %d %d", time, in->pid, in->m->type);
        switch (in->m->type) {
            case BOMBER_MOVE:
                fprintf(file, " %d %d\n", in->m->data.target_position.x, in->m->data.target_position.y);
                break;
            case BOMBER_SEE:
                fprintf(file, "\n");
                break;
            case BOMBER_START:
                fprintf(file, "\n");
                break;
            case BOMBER_PLANT:
                fprintf(file, " %ld %d\n", in->m->data.bomb_info.interval, in->m->data.bomb_info.radius);
                break;
            case BOMB_EXPLODE:
                fprintf(file, "\n");
                break;
        }
    }
    else if ( out ) {
        fprintf(file, "1 %ld %d %d", time, out->pid, out->m->type);
        switch (out->m->type) {
            case BOMBER_LOCATION:
                fprintf(file, " %d %d\n", out->m->data.new_position.x, out->m->data.new_position.y);
                break;
            case BOMBER_DIE:
                fprintf(file, "\n");
                break;
            case BOMBER_VISION:
                fprintf(file, " %d\n", out->m->data.object_count);
                for ( int i=0; i<out->m->data.object_count; i++) {
                    fprintf(file, "%d %d %d %d\n", i+1,
                           objects[i].position.x, objects[i].position.y,
                           objects[i].type);
                }
                break;
            case BOMBER_PLANT_RESULT:
                fprintf(file, " %d\n", out->m->data.planted);
                break;
            case BOMBER_WIN:
                fprintf(file, "\n");
                break;
        }
    }
    else if ( obstacle ){
        fprintf(file, "2 %ld %d %d %d\n", time, obstacle->position.x, obstacle->position.y, obstacle->remaining_durability);
    }
    else {
        perror("One of the parameters needs to be filled.");
    }
    fflush(file);
}

