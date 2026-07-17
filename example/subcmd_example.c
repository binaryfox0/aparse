#include <stdio.h>
#include <aparse.h>
#include <math.h>

static int g_result_only = 0;
typedef struct
{
    int a;
    int b;
} add_payload_t;

static int count_digits(size_t n)
{
    int digits = 0;
    while(n > 0)
    {
        digits++;
        n /= 10;
    }
    return digits;
}

static void add_command(const aparse_arg *arg, void *data)
{
    add_payload_t *p = data;
    (void)arg;

    if(!g_result_only)
    {
        aparse_prog_info("result: %d + %d = %d", 
                p->a, p->b, p->a + p->b);
    } else {
        aparse_prog_info("result: %d", 
                p->a + p->b);
    }
}

static void poly_surf_command(const aparse_arg *arg, void *data)
{
    aparse_list list = *(aparse_list*)data;
    double *points = list.ptr;
    size_t point_count = list.size / 2;
    int dig_count = count_digits(point_count);
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
        if(!g_result_only)
        {
            aparse_prog_info("%*zu: (%.6f, %.6f)", 
                    dig_count, i + 1, points[i * 2], points[i * 2 + 1]);
        }
        size_t j = (i + 1) % point_count;
        total_area += points[i * 2] * points[j * 2 + 1];
        total_area -= points[j * 2] * points[i * 2 + 1];
    }
    if(total_area == 0)
    {
        aparse_prog_error("not a valid polygon");
        return;
    }
    
    if(g_result_only)
    {
        aparse_prog_info("vertices order: %s", 
                total_area > 0 ? "counter-clockwise" : "clockwise");
    }
    aparse_prog_info("total surface: %.6f",
            (total_area < 0 ? -total_area : total_area) / 2);
}

static void poly_perm_command(const aparse_arg *arg, void *data)
{
    aparse_list list = *(aparse_list*)data;
    double *points = list.ptr;
    size_t point_count = list.size / 2;
    int dig_count = count_digits(point_count);
    double perm = 0;
    
    (void)arg;

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
       
        if(!g_result_only)
        {
            aparse_prog_info("%*zu: (%.6f, %.6f)", 
                    dig_count, i + 1, points[i * 2], points[i * 2 + 1]);
        }
        perm += sqrt((double)(dx*dx+dy*dy));
    }
    aparse_prog_info("total perimitter: %.6f", perm);
}

static void nothing_command(const aparse_arg *arg, void *data)
{
    (void)arg;
    (void)data;
    aparse_prog_info("this do nothing, seriously");
}

int main(int argc, char **argv)
{
    aparse_arg add_args[] =
    {
        aparse_arg_number("a", 0, sizeof(int), 
                APARSE_ARG_TYPE_SIGNED, "First integer in equation"),
        aparse_arg_number("b", 0, sizeof(int), 
                APARSE_ARG_TYPE_SIGNED, "Second integer in equation"),
        aparse_arg_end_marker
    };

    aparse_arg poly_surf_args[] = 
    {
        aparse_arg_array("points", 0, sizeof(aparse_list), 0, APARSE_ARG_TYPE_FLOAT, sizeof(double), "List of points in format x, y"),
        aparse_arg_end_marker
    };
    
    aparse_arg subcommand_list[] = {
        aparse_arg_subparser("add", add_args, add_command,
                NULL, 0,
                "Add two integer together",
                add_payload_t, a, b),
        aparse_arg_subparser_impl("poly-surf", poly_surf_args, 
                poly_surf_command, NULL, 0,
                "Calculate a polygon surface area from given list of points", 
                (size_t[]){0, sizeof(aparse_list)}, 1),
        aparse_arg_subparser_impl("poly-perm", poly_surf_args, 
                poly_perm_command, NULL, 0,
                "Calculate a polygon perimitter length from given list of points", 
                (size_t[]){0, sizeof(aparse_list)}, 1),
        aparse_arg_subparser("nothing", NULL, nothing_command,
                NULL, 0, "This do nothing literally"),
        aparse_arg_end_marker
    };
    aparse_arg main_args[] = {
        aparse_arg_parser("command", subcommand_list),
        aparse_arg_option(0, "--result-only", &g_result_only, sizeof(g_result_only),
                APARSE_ARG_TYPE_BOOL, "Only print the result"),
        aparse_arg_end_marker
    };
    if(aparse_parse(
                argc, argv, 
                main_args, NULL,
                "Demonstration for subcommands") != APARSE_STATUS_OK)
        return 1;
    return 0;
}
