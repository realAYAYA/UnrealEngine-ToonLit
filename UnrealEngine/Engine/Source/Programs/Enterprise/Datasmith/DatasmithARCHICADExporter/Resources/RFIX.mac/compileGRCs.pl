#! /usr/bin/perl -w

use FindBin ;
use File::Spec::Functions qw(splitpath catfile updir);
use File::Copy;
use Cwd;
use feature 'unicode_strings';

#-----------------------------------------------------------------------
# Executes one GRC conversion command
#-----------------------------------------------------------------------
sub DoGRC ($$$$$$$)
{
	my ($tool, $headerDir, $sourceDir, $intSourceDir, $inputFile, $destinationDir, $outputFile) = @_ ;

    if (system ("mkdir -p \"" . $destinationDir . "\"") != 0) {
        print "Fail: mkdir -p \"" . $destinationDir . "\"";
        exit(1)
    };

    system ("vim -e -s +\"set bomb|set encoding=utf-8|wq\" \"" . catfile ($sourceDir, $inputFile) . "\"");

    if (system ("xcrun clang -x c++ -E -P " . $ENV{GRC_DEFINES} . " -I \"" . $headerDir . "\" -I \"" . $intSourceDir . "\" \"" . catfile ($sourceDir, $inputFile) . "\" > \"" . catfile ($destinationDir, $outputFile .".i") . "\"") != 0) {
        print "Fail: xcrun clang -x c++ -E -P " . $ENV{GRC_DEFINES} . " -I \"" . $headerDir . "\" -I \"" . $intSourceDir . "\" \"" . catfile ($sourceDir, $inputFile) . "\" > \"" . catfile ($destinationDir, $outputFile .".i") . "\"";
        exit(1)
    }
    if (system ($tool . " -m r -q utf8 utf16 -T M -n -i \"" . catfile ($destinationDir, $outputFile .".i") . "\" -p \"" . catfile ($sourceDir, "Images") . "\" -o \"" . catfile ($destinationDir, $outputFile) . "\" -w 2") != 0) {
        print "Fail: " . $tool . " -m r -q utf8 utf16 -T M -n -i \"" . catfile ($destinationDir, $outputFile .".i") . "\" -p \"" . catfile ($sourceDir, "Images") . "\" -o \"" . catfile ($destinationDir, $outputFile) . "\" -w 2";
        exit(1)
    }
}


#-----------------------------------------------------------------------
# Convert GRC
#-----------------------------------------------------------------------
print "Converting resource files:\n";

my $devKitPath = $ENV{AC_SDK_PATH};
my $makeResConvTool = "\"" . catfile (Cwd::realpath ($devKitPath), "Support", "Tools", "OSX", "ResConv") . "\"";

my $projectPath = Cwd::realpath (catfile ($FindBin::Bin, updir ()));

my $resTarget = catfile ($ENV{TARGET_BUILD_DIR}, $ENV{UNLOCALIZED_RESOURCES_FOLDER_PATH});
my $locResTarget = catfile ($resTarget, "English.lproj");
my $roFolder = catfile ($ENV{TARGET_BUILD_DIR}, "RO");
my $rfixFolder = catfile ($projectPath, "RFIX");
my $rintFolder = catfile ($projectPath, "RINT");
my $srcFolder = catfile ($projectPath, "../Private");
my @files;

opendir (DIR, $rfixFolder);
@files = readdir (DIR);
closedir (DIR);

foreach $path (@files) {
	($_,$_,$file) = splitpath ($path);
	DoGRC ($makeResConvTool, $srcFolder, $rfixFolder, $rintFolder, $file, $roFolder, $file . ".ro") if ($file =~ /.*\.grc$/i);
}

opendir (DIR, $roFolder);
@files = readdir (DIR);
closedir (DIR);

system ("mkdir -p \"" . $locResTarget . "\"") ;

foreach $file (@files) {
	if ($file =~ /.*\.tif$/) {
		copy (catfile ($roFolder, $file), $resTarget);
	} elsif ($file =~ /.*\.rsrd$/) {
		copy (catfile ($roFolder, $file), $locResTarget);
	}
}

my $locStrings = catfile ($locResTarget, "Localizable.strings");

# create the output Localizable.strings file
open (OFILE, ">:encoding(UTF-16LE)", $locStrings) or die "Can't create $locStrings: $!";
my $buf = "\x{feff}";
print OFILE $buf;

printf "\t=> Localizable.strings\n";

# collect all files from the RO folder
opendir (DIR, $roFolder);
@files = readdir (DIR);
closedir (DIR);

# enable slurp mode
undef $/;

foreach $file (@files) {
	if ($file =~ /.*\.strings$/) {
		printf "\t\t$file\n";
		my $stringsFile = catfile ($roFolder, $file);
		if ((stat ($stringsFile))[7] > 2) {		# check file size
			open (IFILE, "<:encoding(UTF-16LE)", $stringsFile) or die "Can't open $stringsFile: $!";

			# skip byte order mark
			read (IFILE, $buf, 1);
			if ($buf ne "\x{feff}") {
				print OFILE $buf;
			}

			my $content = <IFILE>;
			close (IFILE);

			print OFILE $content;
		}
	}
}

close (OFILE) or die "Can't close $locStrings: $!";

print "done.\n";
