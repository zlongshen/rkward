function calculate(){
	var file = getValue("file");
	// read in variables from dialog
	var ascii = getValue("ascii");
	var compress = getValue("compress");
	var complevel = getValue("complevel");
	var xzextreme = getValue("xzextreme");

	// the R code to be evaluated
	var data = getValue("data").split("\n").join(", ");
	echo("save(" + data);
	if(file) {
		echo(",\n\tfile=\"" + file + "\"");
	}
	if(ascii) {
		echo(",\n\tascii=TRUE");
	}
	if(compress) {
		if(compress != "xz" | xzextreme != "true") {
			echo(",\n\tcompress=\"" + compress + "\",\n\tcompression_level=" + complevel);
		} else if(xzextreme) {
			echo(",\n\tcompress=\"" + compress + "\",\n\tcompression_level=-" + complevel);
		}
	}
	echo(")\n\n");
}

function printout(){
	new Header (i18n ("Save R objects")).addFromUI ("file").addFromUI ("data").print ();
}

