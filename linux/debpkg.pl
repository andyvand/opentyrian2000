#!/usr/bin/perl

use strict;
use File::Basename;

my $dirname = dirname(__FILE__);

my $VERSION="3.0.0";
my $INSTALL_PREFIX="/tmp/opentyrian2000";
my $CTL_FILENAME="$INSTALL_PREFIX/DEBIAN/control";
my $ARCH="amd64";
my $PKG_OUTPUT_FILE="opentyrian2000-$VERSION-$ARCH.deb";

# Start by auto figuring out dependencies of the executable.
# the rest of the package creation is trival.
my $SO_LIST="";
$SO_LIST=`objdump -x  $INSTALL_PREFIX/usr/bin/opentyrian2000`;
#$SO_LIST= $SO_LIST . `objdump -x  $INSTALL_PREFIX/usr/bin/fceux-gtk`;

#print "$SO_LIST";

my $i; my $j; my $k; my $pkg;
my @fd = split /\n/, $SO_LIST;
my @libls;

$#libls=0;

for ($i=0; $i<=$#fd; $i++)
{
   #$fd[$i] =~ s/^\s+//;
   #print "$fd[$i]\n";

   if ( $fd[$i] =~ m/NEEDED\s+(.*)/ )
   {
      #print "$1 \n";
      $libls[$#libls] = $1; $#libls++;
   }

}

my %pkghash; my $pkgsearch;
my @pkglist; my @pkgdeps;
my $pkg; my $filepath;

$#pkgdeps=0;

for ($i=0; $i<$#libls; $i++)
{
   $pkgsearch=`dpkg-query -S  $libls[$i]`;

   @pkglist = split /\n/, $pkgsearch;

   for ($j=0; $j<=$#pkglist; $j++)
   {
      #$pkghash{$pkg} = 1;
      #print "  $libls[$i]    '$pkglist[$j]'  \n";

      if ( $pkglist[$j] =~ m/(.*):$ARCH:\s+(.*)/ )
      {
	       $pkg = $1;
          $filepath = $2;
          
          $filepath =~ s/^.*\///;
          
	       if ( $libls[$i] eq $filepath )
	       {
             #print "PKG:   '$pkg'   '$libls[$i]' ==  '$filepath' \n";
             # Don't put duplicate entries into the pkg depend list.
             if ( !defined( $pkghash{$pkg} ) )
             {
                $pkgdeps[ $#pkgdeps ] = $pkg; $#pkgdeps++;
                $pkghash{$pkg} = 1;
             }
          }
      }
   }
}
#
#
system("mkdir -p $INSTALL_PREFIX/DEBIAN");

open CTL, ">$CTL_FILENAME" or die "Error: Could not open file '$CTL_FILENAME'\n";
#
print CTL "Package: opentyrian2000\n";
print CTL "Version: $VERSION\n";
print CTL "Section: games\n";
print CTL "Priority: extra\n";
print CTL "Architecture: $ARCH\n";
print CTL "Homepage: http://github.com/andyvand/opentyrian2000\n";
print CTL "Essential: no\n";
#print CTL "Installed-Size: 1024\n";
print CTL "Maintainer: Andy Vandijck\n";
print CTL "Description: opentyrian2000 is an arcade shooter game\n";
print CTL "Depends: $pkgdeps[0]";

for ($i=1; $i<$#pkgdeps; $i++)
{
   print CTL ", $pkgdeps[$i]";
}
print CTL "\n";

close CTL;

#system("cat $CTL_FILENAME");
#
chdir "/tmp";
system("dpkg-deb  --build  opentyrian2000 ");
if ( !(-e "/tmp/opentyrian2000.deb") )
{
   die "Error: Failed to create package $PKG_OUTPUT_FILE\n";
}
system("mv  opentyrian2000.deb  $PKG_OUTPUT_FILE");
system("dpkg-deb  -I   $PKG_OUTPUT_FILE");
#
if ( -e "/tmp/$PKG_OUTPUT_FILE" )
{ 
   print "**********************************************\n";
   print "Created deb package: /tmp/$PKG_OUTPUT_FILE\n";
   print "**********************************************\n";
}
else
{
   die "Error: Failed to create package $PKG_OUTPUT_FILE\n";
}
