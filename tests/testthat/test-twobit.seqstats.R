
test_that("twobit.seqlengths()", {
    filepath <- system.file(package="Rtwobitlib", "extdata", "eboVir3.2bit")
    result <- twobit.seqlengths(filepath)
    expect_identical(result, c(KM034562v1=18957L))
})

