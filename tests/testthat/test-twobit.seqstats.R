
test_that("twobit.seqstats()", {
    filepath <- system.file(package="Rtwobitlib", "extdata", "eboVir3.2bit")
    result <- twobit.seqstats(filepath)
    expected <- rbind(KM034562v1=c(seqlengths=18957L,
                                   A=6051L, C=4050L, G=3756L, T=5100L, N=0L))
    expect_identical(result, expected)

    result <- twobit.seqlengths(filepath)
    expect_identical(result, c(KM034562v1=18957L))
})

