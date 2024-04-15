\name{twobit.seqstats}

\alias{twobit.seqstats}
\alias{twobit.seqlengths}

\title{Extract sequence lengths and base counts from a .2bit file}

\description{
  Extract the lengths and base counts of the DNA sequences stored
  in a .2bit file.
}

\usage{
twobit.seqstats(filepath)

twobit.seqlengths(filepath)
}

\arguments{
  \item{filepath}{
    A single string (character vector of length 1) containing a path
    to a .2bit file.
  }
}

\details{
  \code{twobit.seqlengths(filepath)} is a shortcut for
  \code{twobit.seqstats(filepath)[ , "seqlengths"]} that is also a
  much more efficient way to get the sequence lengths as it does not
  need to load the sequence data in memory.
}

\value{
  For \code{twobit.seqstats()}: An integer matrix with one row per sequence
  in the .2bit file and 6 columns. The rownames on the matrix are the sequence
  names and the colnames are: \code{seqlengths}, \code{A}, \code{C}, \code{G},
  \code{T}, \code{N}. Columns \code{A}, \code{C}, \code{G}, \code{T}, and
  \code{N} contain the letter count for each sequence.

  For \code{twobit.seqlengths()}: A named integer vector where the names
  are the sequence names and the values the corresponding lengths.
}

\references{
  A quick overview of the .2bit format:
  \url{https://genome.ucsc.edu/FAQ/FAQformat.html#format7}
}

\examples{
filepath <- system.file(package="Rtwobitlib", "extdata", "eboVir3.2bit")

twobit.seqstats(filepath)

twobit.seqlengths(filepath)
}

\keyword{manip}
