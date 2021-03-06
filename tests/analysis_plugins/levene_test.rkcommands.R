local({
## Prepare
require(car)
## Compute
result <- leveneTest (warpbreaks[["breaks"]], warpbreaks[["tension"]])
## Print result
names <- rk.get.description (warpbreaks[["breaks"]], warpbreaks[["tension"]])

rk.header ("Levene's Test", parameters=list("response variable"=names[1],
	"groups"=names[2]))

rk.print (result)
})
