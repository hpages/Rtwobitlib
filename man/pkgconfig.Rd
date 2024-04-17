\name{pkgconfig}

\alias{pkgconfig}

\title{Compiler configuration arguments for use of Rtwobitlib}

\description{
  This function prints values for \code{PKG_LIBS} and
  \code{PKG_CPPFLAGS} variables for use in Makevars files.
  See \code{vignette("Rtwobitlib")} for details.
}

\usage{
pkgconfig(opt=c("PKG_LIBS", "PKG_CPPFLAGS"))
}

\arguments{
  \item{opt}{A scalar character from the list of available options;
             default is \code{PKG_LIBS}.}
}

\value{
  The function prints the \code{PKG_LIBS} or \code{PKG_CPPFLAGS} value
  and returns an invisible \code{NULL}.
}

\examples{
pkgconfig("PKG_LIBS")

pkgconfig("PKG_CPPFLAGS")
}

\keyword{manip}
