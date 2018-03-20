if [ ! -s include/asterisk.h ] ; then
	echo "please cd into the directory where the asterisk source has been untarred\n"
	exit
fi
cp asterisk-res_redis/res_redis.c res/.
cp asterisk-res_redis/res_redis.conf.sample configs/.
echo "... done; please follow the next steps in the README file"

