/*
 * getoptx.c - extended getopt(s) builtin using getopt_long(3)
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 2017 dana
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall dana or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if dana and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * dana and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and dana and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

#include <getopt.h>

#include "getoptx.mdh"
#include "getoptx.pro"

#if defined(__GLIBC__) || defined(LIBC_MUSL)
extern char *__progname;
#endif

#define RET_OK      0
#define RET_LIB_ERR 1
#define RET_BIN_ERR 2

/**
 * Portably get the current program name, if applicable.
 *
 * @return The current program name. May be NULL on some systems.
 */

static char *
get_progname(void)
{
#if defined(__CYGWIN__) || defined(__UCLIBC__)
    return NULL;
#elif defined(__GLIBC__) || defined(LIBC_MUSL)
    return __progname;
#else
    return (char *) getprogname();
#endif
}

/**
 * Portably set the current program name, if applicable.
 *
 * @param The name to set.
 */

static void
set_progname(const char *name)
{
#if defined(__CYGWIN__) || defined(__UCLIBC__)
    return;
#elif defined(__GLIBC__) || defined(LIBC_MUSL)
    __progname = (char *) name;
#else
    setprogname(name);
#endif
}

/**
 * Strip any punctuation from a string and return the result.
 *
 * @param str The string to strip.
 *
 * @return The input string minus any punctuation. Caller must free.
 */

static char *
strip_punct(const char *str)
{
    int         i      = 0;
    char       *newstr = zshcalloc(strlen(str) + 1);
    const char *ptr    = str;

    for ( /**/; *ptr && *ptr != '\0'; ptr++ ) {
        if ( ! ispunct(*ptr) ) {
            newstr[i++] = *ptr;
        }
    }

    return newstr;
}

/**
 * Build up parsed arg string.
 *
 * @param argstr Current (pre-allocated) arg string. Will be re-allocated.
 * @param fmt    A format string suitable for printf(), &al.
 * @param ...    Zero or more values to be used with the format string.
 *
 * @return 0 on success, >0 on error.
 */

static int
build_argstr(char **argstr, const char *fmt, ...)
{
    int      buf_len = 32;
    int      left    = -1;
    char    *buf     = zshcalloc(buf_len);
    va_list  args;

    va_start(args, fmt);

    left = vsnprintf(buf, buf_len, fmt, args);

    if ( left >= buf_len ) {
        buf  = realloc(buf, left);
        left = vsnprintf(buf, left, fmt, args);
    }

    va_end(args);

    if ( left < 0 ) {
        free(buf);
        return 1;
    }

    *argstr = appstr(*argstr, buf);
    free(buf);
    return 0;
}

/**
 * Add a long option to a long-options array.
 *
 * @param longopts
 *   The long-options array to update. Will be re-allocated if necessary.
 *
 * @param name
 *   The long-option name, with any special characters removed. A name with a
 *   leading `-` or trailing `:` is invalid.
 *
 * @param has_arg
 *   Whether the option takes an argument. Refer to getopt_long(3) for possible
 *   values.
 *
 * @return 0 on success, >0 on error.
 */

static int
add_longopt(struct option **longopts, const char *name, int has_arg)
{
    int     i     = -1;
    char   *name2 = ztrdup(name);
    size_t  len   = 0;

    struct option *ptr;

    static int flag;

    len = strlen(name2);

    if ( len == 0 || name2[0] == '-' || name2[len - 1] == ':' ) {
        return 1;
    }

    // Option already exists — update in place
    for ( ptr = *longopts; ptr; ptr++, i++ ) {
        if ( ptr->name && !strcmp(name2, ptr->name) ) {
            ptr->has_arg = has_arg;
            return 0;
        }
        if ( !ptr->name ) {
            break;
        }
    }

    i++;

    *longopts = realloc(*longopts, sizeof(struct option) * (i + 2));

    // Note: The `val` must be different for each option, or else
    // ambiguous-option detection will behave strangely (i.e., fail). Also, when
    // we do this, we must provide a flag too
    (*longopts)[i].name        = name2;
    (*longopts)[i].has_arg     = has_arg;
    (*longopts)[i].flag        = &flag;
    (*longopts)[i].val         = i + 1;

    (*longopts)[i + 1].name    = NULL;
    (*longopts)[i + 1].has_arg = 0;
    (*longopts)[i + 1].flag    = NULL;
    (*longopts)[i + 1].val     = 0;

    return 0;
}

/**
 * Add long options from a long-option spec provided at the command line.
 *
 * @param longopts
 *   The long-options array to update. Will be passed to add_longopt() where
 *   necessary.
 *
 * @param optspec
 *   The long-option spec provided at the command line. Multiple long options
 *   may be separated with white space, commas, or pipes. Each long option may
 *   optionally begin with two hyphens. A long option ending with a single colon
 *   indicates a required argument, whilst two colons indicate an optional
 *   argument.
 *
 * @param norm_punct
 *   Whether to normalise punctuation in the added options. If >0 and an option
 *   contains punctuation (e.g., `foo-bar`), an additional option will be added
 *   without the punctuation.
 *
 * @return 0 on success, >0 on error.
 */

static int
add_longopts(struct option **longopts, const char *optspec, int norm_punct) {
    int   ret      = 0;
    char *optspec2 = ztrdup(optspec);
    char *tokenp   = strtok(optspec2, " \r\n\t|,");

    while ( tokenp ) {
        int    has_arg = no_argument;
        size_t len     = strlen(tokenp);

        if ( len >= 3 && tokenp[0] == '-' && tokenp[1] == '-' ) {
            tokenp += 2;
            len    -= 2;
        }

        if ( len >= 3 && tokenp[len - 2] == ':' && tokenp[len - 1] == ':' ) {
            has_arg          = optional_argument;
            tokenp[len - 2]  = '\0';
            tokenp[len - 1]  = '\0';
            len             -= 2;
        } else if ( len >= 2 && tokenp[len - 1] == ':' ) {
            has_arg          = required_argument;
            tokenp[len - 1]  = '\0';
            len             -= 1;
        }

        if ( len ) {
            if ( add_longopt(longopts, tokenp, has_arg) ) {
                ret++;

            // Add punctuationless version, if applicable
            } else if ( norm_punct ) {
                char *nptokenp = strip_punct(tokenp);

                if ( strlen(tokenp) > strlen(nptokenp) ) {
                    if ( add_longopt(longopts, nptokenp, has_arg) ) {
                        ret++;
                    }
                }

                free(nptokenp);
            }
        }

        tokenp = strtok(NULL,  " \r\n\t|,");
    }

    free(optspec2);
    return ret;
}

/**
 * Implement `getoptx` built-in.
 *
 * usage: getoptx [<options>] <shortopts> [<arg> ...]
 *
 * options:
 *   -A <array>    Assign result to array parameter <array>
 *   -c            Concatenate adjacent, same-optind numeric short options
 *   -e            Omit errors in result (like getopt(1))
 *   -E            Abort immediately on parse error
 *   -l <longopt>  Define long option(s)
 *   -n <name>     Set name used for error messages
 *   -p            Normalise punctuation in long options
 *   -q            Suppress error messages (same effect as prefixing <shortopts>
 *                 with `:`)
 *   -s <scalar>   Assign result to scalar parameter <scalar>
 *
 * operands:
 *   <shortopts>   Short-option spec string. This argument is required; use an
 *                 empty string if no short options should be defined
 *   <arg> ...     Zero or more arguments to use as input; positional parameters
 *                 are used if not supplied
 *
 * return codes:
 *   0  Argument parsing was successful.
 *   1  Argument parsing failed.
 *   2  An illegal option was provided to the built-in itself, or some other
 *      usage or internal error occurred.
 */

/**/
static int
bin_getoptx(char *nam, char **args, UNUSED(Options ops), UNUSED(int func))
{
    int ret             = RET_OK;
    int concat_nums     = 0; // -c
    int err_elide       = 0; // -e
    int err_abort       = 0; // -E
    int norm_punct      = 0; // -p
    int longoptsi       = 0;
    int argc            = 0;
    int opt             = -1;
    int longind         = -1;
    int was_num         = 0;
    int this_optind     = 1;
    int last_num_optind = -1;

    char  *arrname   = NULL; // -A
    char  *longopt   = NULL; // -l
    char  *name      = NULL; // -n
    char  *scaname   = NULL; // -s
    char  *arg       = NULL;
    char  *shortopts = NULL;
    char  *argstr    = zshcalloc(1);
    char **longoptsp = zshcalloc(8 * sizeof(char *));
    char **argp      = NULL;
    char **argv      = NULL;

    const char *old_progname = get_progname();

    struct option *longopts = zshcalloc(sizeof(struct option));

    for ( /**/; *args && **args == '-'; args++ ) {
        arg = *args + 1;

        if ( !*arg || (*arg == '-' && strlen(arg) == 1) ) {
            args++;
            break;
        }

        for ( /**/; *arg; arg++ ) {
            switch ( *arg ) {
                // Assign parsed arguments to array
                case 'A':
                    if ( arg[1] ) {
                        arrname = arg + 1;
                    } else if ( !(arrname = *++args) ) {
                        ret = RET_BIN_ERR;
                        goto internal_optarg_expected;
                    }
                    goto internal_next_arg;

                // Concatenate adjacent numeric short options
                case 'c':
                    concat_nums = 1;
                    break;

                // Elide errors (? and :) in parsed arguments
                case 'e':
                    err_elide = 1;
                    break;

                // Abort immediately on parse error
                case 'E':
                    err_abort = 1;
                    break;

                // Add long option spec
                case 'l':
                    if ( arg[1] ) {
                        longopt = arg + 1;
                    } else if ( !(longopt = *++args) ) {
                        ret = RET_BIN_ERR;
                        goto internal_optarg_expected;
                    }
                    // @todo Is this stupid? lol
                    if ( longoptsi && longoptsi % 8 == 0 ) {
                        longoptsp = realloc(longoptsp, (longoptsi + 8) * sizeof(char *));
                    }
                    longoptsp[longoptsi++] = longopt;
                    goto internal_next_arg;

                // Set name used in error messages
                case 'n':
                    if ( arg[1] ) {
                        name = arg + 1;
                    } else if ( !(name = *++args) ) {
                        ret = RET_BIN_ERR;
                        goto internal_optarg_expected;
                    }
                    goto internal_next_arg;

                // Normalise long-option punctuation
                case 'p':
                    norm_punct = 1;
                    break;

                // Suppress error messages
                case 'q':
                    opterr = 0;
                    break;

                // Assign parsed arguments to scalar
                case 's':
                    if ( arg[1] ) {
                        scaname = arg + 1;
                    } else if ( !(scaname = *++args) ) {
                        ret = RET_BIN_ERR;
                        goto internal_optarg_expected;
                    }
                    goto internal_next_arg;

                // Illegal option
                default:
                    ret = RET_BIN_ERR;
                    goto internal_option_invalid;
            }

            continue;
            internal_next_arg: break;
        }
    }

    // Add long options, accounting for norm_punct
    while ( --longoptsi >= 0 ) {
        if ( add_longopts(&longopts, *longoptsp, norm_punct) ) {
            ret = RET_BIN_ERR;
            goto internal_longopt_invalid;
        }
        longoptsp++;
    }

    // Missing short optspec
    if ( !*args ) {
        ret = RET_BIN_ERR;
        goto internal_operand_expected;
    }

    // Prepend : to short-opt spec if we got -q
    if ( opterr == 0 && **args != ':' ) {
        shortopts = bicat(":", *args);
    } else {
        if ( **args == ':' ) {
            opterr = 0;
        }
        shortopts = ztrdup(*args);
    }

    // If we're concatenating numeric options we should just make sure they're
    // always present in the optspec; why else would someone use this option?
    if ( concat_nums && !strpbrk(shortopts, "0123456789") ) {
        shortopts = appstr(shortopts, "0123456789");
    }

    if ( !name ) {
        name = scriptname ? scriptname : (argzero ? argzero : nam);
    }

    set_progname(name);

    // Use positional parameters if none were supplied to the command
    argp = args[1] ? args + 1 : pparams;
    argv = (char **) zshcalloc((arrlen(argp) + 1) * sizeof(char *));

    argv[0] = name;

    for ( int i = 1; *argp; i++, argc++ ) {
        argv[i] = *argp++;
    }
    argc++;

    this_optind = optind = 1;

    for ( ;; ) {
        opt = getopt_long(argc, argv, shortopts, longopts, &longind);

        if ( opt < 0 ) {
            break;
        }

        // If we're concatenating numbers, we need to be able to accurately
        // keep track of the 'current' optind. Since operands interspersed with
        // options are skipped in the loop, we can sometimes lose this,
        // appearing to be 1 or 2 or 20 optinds behind. To account for this,
        // we can scan forward from the last optind we knew about and skip any
        // arguments that don't look like options
        if ( concat_nums && optind != this_optind ) {
            while ( argv[this_optind][0] != '-' ) {
                this_optind++;
            }
        }

        // Missing argument
        if ( opt == ':' ) {
            ret = RET_LIB_ERR;

            if ( ! err_elide ) {
                build_argstr(&argstr, " ':'");
            }
            if ( err_abort ) {
                goto done;
            }

        // Illegal option
        } else if ( opt == '?' ) {
            ret = RET_LIB_ERR;

            if ( ! err_elide ) {
                build_argstr(&argstr, " '?'");
            }
            if ( err_abort ) {
                goto done;
            }

        // Long option
        } else if ( opt == 0 ) {
            if ( norm_punct ) {
                char *npname = strip_punct(longopts[longind].name);
                build_argstr(&argstr, " --%s", npname);
                free(npname);
            } else {
                build_argstr(&argstr, " --%s", longopts[longind].name);
            }

        // Short option
        } else {
            // Concatenate same-optind adjacent digit options, if applicable
            if ( concat_nums && isdigit(opt) ) {
                if ( was_num && last_num_optind == this_optind ) {
                    build_argstr(&argstr, "%c", opt);
                } else {
                    build_argstr(&argstr, " -%c", opt);
                }

                was_num         = 1;
                last_num_optind = this_optind;

            } else {
                was_num = 0;
                build_argstr(&argstr, " -%c", opt);
            }
        }

        this_optind = optind;

        if ( optarg ) {
            build_argstr(&argstr, " '%s'", quotestring(optarg, QT_SINGLE));
        }
    }

    build_argstr(&argstr, " --");

    while ( optind < argc ) {
        build_argstr(&argstr, " '%s'", quotestring(argv[optind++], QT_SINGLE));
    }

    goto done;

    internal_operand_expected:
        zwarnnam(nam, "not enough arguments");
        goto done;

    internal_option_invalid:
        zwarnnam(nam, "bad option: -%s", arg);
        goto done;

    internal_optarg_expected:
        zwarnnam(nam, "argument expected after -%c option", *arg);
        goto done;

    internal_longopt_invalid:
        zwarnnam(nam, "empty or illegal long option spec: %s", *longoptsp);
        goto done;

    done:
        // Discard result if we have err_abort and an error
        if ( *argstr && (err_abort && ret) ) {
            argstr[0] = '\0';
        }

        // Assign to array
        if ( arrname ) {
            // Check for identifier validity down here to match the way it works
            // with scalars
            if ( !isident(arrname) ) {
                // The wording/behaviour here matches assignaparam(), &al.
                zerr("not an identifier: %s", arrname);
                errflag |= ERRFLAG_ERROR;
                ret = RET_BIN_ERR;
            } else {
                char *tmp = zshcalloc(
                    strlen(argstr) + strlen(arrname) + strlen("=(...)") + 1
                );
                char **tmp2 = zshcalloc(2 * sizeof(char *));

                sprintf(tmp, "%s=( %s )", arrname, argstr + 1);
                *tmp2 = tmp;

                if ( bin_eval(NULL, tmp2, NULL, 0) ) {
                    ret = 2;
                }

                free(tmp);
                free(tmp2);
            }

        // Assign to scalar
        } else if ( scaname ) {
            if ( !setsparam(scaname, ztrdup(argstr + 1)) ) {
                ret = RET_BIN_ERR;
            }

        // Print to stdout
        } else if ( *argstr ) {
            fprintf(stdout, "%s\n", argstr + 1);
        }

        set_progname(old_progname);

        if ( argv ) {
            free(argv);
        }
        if ( shortopts ) {
            free(shortopts);
        }
        if ( longoptsp ) {
            //free(longoptsp); // @todo Why can't i free this?
        }
        if ( longopts ) {
            free(longopts); // @todo Think i need to free the names too
        }
        if ( argstr ) {
            free(argstr);
        }

        return ret;
}

/* module paraphernalia */

static struct builtin bintab[] = {
    BUILTIN("getoptx", BINF_HANDLES_OPTS, bin_getoptx, 0, -1, 0, NULL, NULL),
};

static struct features module_features = {
    bintab, sizeof(bintab) / sizeof(*bintab),
    NULL, 0,
    NULL, 0,
    NULL, 0,
    0
};

/**/
int
setup_(UNUSED(Module m))
{
    return 0;
}

/**/
int
features_(Module m, char ***features)
{
    *features = featuresarray(m, &module_features);
    return 0;
}

/**/
int
enables_(Module m, int **enables)
{
    return handlefeatures(m, &module_features, enables);
}

/**/
int
boot_(UNUSED(Module m))
{
    return 0;
}

/**/
int
cleanup_(Module m)
{
    return setfeatureenables(m, &module_features, NULL);
}

/**/
int
finish_(UNUSED(Module m))
{
    return 0;
}
