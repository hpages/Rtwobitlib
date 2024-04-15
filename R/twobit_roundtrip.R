twobit_write <- function(x, filepath)
{
    ## Check 'x'.
    if (!is.character(x))
        stop("'x' must be a character vector")

    ## Check 'names(x)'.
    x_names <- names(x)
    if (is.null(x_names))
        stop("'x' must be a named character vector")
    if (anyNA(x_names) || !all(nzchar(x_names)) || anyDuplicated(x_names))
        stop("names on 'x' cannot contain NAs, empty strings, or duplicates")

    filepath <- normarg_filepath(filepath)
    .Call("C_twobit_write", filepath, PACKAGE="Rtwobitlib")
}

twobit_read <- function(filepath)
{
    filepath <- normarg_filepath(filepath)
    .Call("C_twobit_read", filepath, PACKAGE="Rtwobitlib")
}

