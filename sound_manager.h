#ifndef __SOUND_MANAGER_H__
#define __SOUND_MANAGER_H__

#include <stdio.h>
#include <string>

struct playObj
{
	std::string 	url;
	bool 			isLoop;
	static playObj* builder(std::string url, bool isLoop)
	{
		playObj* po = new playObj;
		po->url = url;
		po->isLoop = isLoop;
		return po;
	}
};

class sound_manager
{
public:
    sound_manager();
    ~sound_manager();

    static void init();
    static int load(std::string url);
    static int play(playObj* po);
    static void close();
private:
};


#endif
