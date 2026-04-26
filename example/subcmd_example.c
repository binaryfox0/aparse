#include <stdio.h>
#include <aparse.h>
#include <math.h>

void poly_surf_command(void *args)
{
    aparse_list list = *(aparse_list*)args;
    double *points = list.ptr;
    size_t point_count = list.size / 2;
    int dig_count = floor(log10(point_count)) + 1;
    double total_area = 0;
    if(list.size % 2 != 0)
    {
        aparse_prog_error("missing y component from the final point");
        return;
    }
    if(point_count < 3)
    {
        aparse_prog_error("no polygon existed with %zu points", list.size / 2);
        return;
    }

    for(size_t i = 0; i < point_count; i++)
    {
        aparse_prog_info("%*zu: (%.6f, %.6f)", 
                dig_count, i + 1, points[i * 2], points[i * 2 + 1]);
        size_t j = (i + 1) % point_count;
        total_area += points[i * 2] * points[j * 2 + 1];
        total_area -= points[j * 2] * points[i * 2 + 1];
    }
    if(total_area == 0)
    {
        aparse_prog_error("not a valid polygon");
        return;
    }
    aparse_prog_info("vertices order: %s", 
            total_area > 0 ? "counter-clockwise" : "clockwise");
    aparse_prog_info("total surface: %.6f",
            (total_area < 0 ? -total_area : total_area) / 2);
}

void poly_perm_command(void *args)
{
    aparse_list list = *(aparse_list*)args;
    double *points = list.ptr;
    size_t point_count = list.size / 2;
    int dig_count = floor(log10(point_count)) + 1;
    double perm = 0;
    if(list.size % 2 != 0)
    {
        aparse_prog_error("missing y component from the final point");
        return;
    }
    if(point_count < 3)
    {
        aparse_prog_error("no polygon existed with %zu points", list.size / 2);
        return;
    }

    for(size_t i = 0; i < point_count; i++)
    {
        size_t j = 0;
        double dx = 0, dy = 0;

        j = (i + 1) % point_count;
        dx = points[i * 2] - points[j * 2];
        dy = points[i * 2 + 1] - points[j * 2 + 1];
        
        aparse_prog_info("%*zu: (%.6f, %.6f)", 
                dig_count, i + 1, points[i * 2], points[i * 2 + 1]);
        perm += sqrt((double)(dx*dx+dy*dy));
    }
    aparse_prog_info("total perimitter: %.6f", perm);
}

int main(int argc, char **argv)
{
    aparse_arg poly_surf_args[] = {
        aparse_arg_array("points", 0, sizeof(aparse_list), 0, APARSE_ARG_TYPE_FLOAT, sizeof(double), "List of points in format x, y"),
        aparse_arg_end_marker
    };
    aparse_arg subcommand_list[] = {
        aparse_arg_subparser_impl("poly-surf", poly_surf_args, poly_surf_command, "Calculate a polygon surface area from given list of points", (int[]){0, sizeof(aparse_list)}, 1),
        aparse_arg_subparser_impl("poly-perm", poly_surf_args, poly_perm_command, "Calculate a polygon perimitter length from given list of points", (int[]){0, sizeof(aparse_list)}, 1),
        aparse_arg_end_marker
    };
    aparse_arg main_args[] = {
        aparse_arg_parser("command", subcommand_list),
        aparse_arg_end_marker
    };
    if(aparse_parse(argc, argv, main_args, 0, "Demonstration for subcommands") != APARSE_STATUS_OK)
        return 1;
    return 0;
}
