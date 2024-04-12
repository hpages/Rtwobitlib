\name{twobit.seqlengths}

\alias{twobit.seqlengths}

\title{Extract lengths of sequences stored in a .2bit file}

\description{
  Extract the lengths of the sequences stored in a .2bit file.
}

\usage{
twobit.seqlengths(filepath)
}

\arguments{
  \item{filepath}{
    A single string (character vector of length 1) containing a path
    to a .2bit file.
  }
}

\value{
  A named integer vector where the names are the sequence names and
  the values the corresponding lengths.
}

\references{
  Quick overview of the .2bit format:
  \url{https://genome.ucsc.edu/FAQ/FAQformat.html#format7}
}

\examples{
filepath <- system.file(package="Rtwobitlib", "extdata", "eboVir3.2bit")
twobit.seqlengths(filepath)
}

\keyword{manip}
