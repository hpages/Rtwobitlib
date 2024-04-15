normarg_filepath <- function(filepath)
{
    if (!is.character(filepath) || length(filepath) != 1L || is.na(filepath))
        stop("'filepath' must be a single string")
    filepath <- file_path_as_absolute(filepath)
}

