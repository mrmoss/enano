//Dependencies:
//		-lncurses

#include "editor.hpp"
#include "file.hpp"
#include <chrono>
#include <stdexcept>
#include <thread>

std::vector<std::string> split(const std::string& data,const char delim)
{
	std::vector<std::string> lines;
	std::string line;
	for(auto ii:data)
		if(ii=='\n')
		{
			lines.push_back(line);
			line="";
		}
		else
			line+=ii;
	lines.push_back(line);
	line="";
	return lines;
}

std::string join(const std::vector<std::string>& lines,const char delim)
{
	std::string data;
	for(auto line:lines)
		data+=line+delim;
	if(data.size()>0)
		data.erase(data.size()-1,1);
	return data;
}

editor_t::editor_t():refresh_m(true),stop_m(true),save_func_m(nullptr),
	y_top_margin_m(2),y_bottom_margin_m(2),yoff_m(0),xoff_m(0),cut_moved_m(false),
	modified_str_m("MODIFIED"),saved_str_m("SAVED")
{}

void editor_t::start(const std::string& filename,const std::string& data,std::function<bool(const std::string&)> save_func)
{
	save_func_m=save_func;
	stop_m=false;
	name_m=filename;
	lines_m=split(data,'\n');
	int ch;
	initscr();
	noecho();
	keypad(stdscr,true);
	type_lines(lines_m);
	move(y_top_margin_m,0);
	nodelay(stdscr,true);
	raw();

	while(!stop_m)
	{
		ch=getch();

		if(ch==ERR)
		{
			if(refresh_m)
			{
				resize();
				refresh_m=false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			continue;
		}

		if(ch==KEY_RESIZE)
			refresh_m=true;
		else if(ch==KEY_HOME)
			home();
		else if(ch==KEY_END)
			end();
		else if(ch==KEY_UP)
			move_up();
		else if(ch==KEY_DOWN)
			move_down();
		else if(ch==KEY_PPAGE)
			move_up(std::max(0,max_y()-1));
		else if(ch==KEY_NPAGE)
			move_down(std::max(0,max_y()-1));
		else if(ch==KEY_LEFT)
			move_left();
		else if(ch==KEY_RIGHT)
			move_right();
		else if(ch==KEY_BACKSPACE||ch==127)
			backspace();
		else if(ch==KEY_DC)
			del();
		else if(ch==11)
			cut_line();
		else if(ch==21)
			uncut_lines();
		else if(ch=='\n')
		{
			newline();
			refresh_m=true;
		}
		else if(ch==15&&save_func_m!=nullptr)
		{
			if(save_func_m(join(lines_m,'\n')))
				status_m=saved_str_m;
			else
				status_m="ERROR SAVING FILE";
			refresh_m=true;
		}
		else if(ch=='\t'||(ch>=32&&ch<=126))
		{
			insert_char(ch);
			refresh_m=true;
		}
		else if(ch==3)
		{
			stop();
			exit(0);
		}
	}

	endwin();
}

void editor_t::stop()
{
	stop_m=true;
	endwin();
}

void editor_t::type_lines(const std::vector<std::string>& lines)
{
	if(stop_m)
		return;
	int gx;
	int gy;
	getyx(stdscr,gy,gx);
	move(gy,gx);
	for(auto line:lines)
	{
		type_string(line);
		move(++gy,0);
	}
}

void editor_t::type_string(const std::string& str)
{
	if(stop_m)
		return;
	for(auto ii:str)
		if(ii!='\n')
		{
			if(ii=='\t')
				for(int ii=0;ii<4;++ii)
					addch(' ');
			else
				addch(ii);
		}
	refresh();
}

void editor_t::home()
{
	if(stop_m)
		return;
	int gx;
	int gy;
	getyx(stdscr,gy,gx);
	xoff_m=0;
	move(gy,0);
	refresh_m=true;
}

void editor_t::end()
{
	if(stop_m)
		return;
	int x;
	int y;
	get_pos(y,x);
	int gx;
	int gy;
	getyx(stdscr,gy,gx);
	int w;
	int h;
	getmaxyx(stdscr,h,w);
	bool update=false;
	if((int)lines_m[y].size()>w)
	{
		xoff_m=(int)lines_m[y].size()-w+1;
		update=true;
	}
	else
	{
		if(xoff_m!=0)
			update=true;
		xoff_m=0;
	}
	move(gy,std::min(w-1,(int)lines_m[y].size()));
	if(update)
		refresh_m=true;
}

void editor_t::move_left()
{
	if(stop_m)
		return;
	int x;
	int y;
	get_pos(y,x);
	int gx;
	int gy;
	getyx(stdscr,gy,gx);
	if(gx==0&&xoff_m>0)
	{
		--xoff_m;
		refresh_m=true;
	}
	if(x>0)
	{
		move_pos(y,x-1);
		refresh_m=true;
	}
	else if(y>0)
	{
		move_up();
		end();
	}
}

void editor_t::move_right()
{
	if(stop_m)
		return;
	int x;
	int y;
	get_pos(y,x);
	int gx;
	int gy;
	getyx(stdscr,gy,gx);
	int w;
	int h;
	getmaxyx(stdscr,h,w);
	if(gx==w-1&&x+1<=(int)lines_m[y].size())
	{
		++xoff_m;
		refresh_m=true;
	}
	if(x+1<=(int)lines_m[y].size())
	{
		move_pos(y,x+1);
		refresh_m=true;
	}
	else if(y+1<(int)lines_m.size())
	{
		move_down();
		home();
	}
}

void editor_t::move_up(const int times)
{
	if(stop_m)
		return;
	int w;
	int h;
	getmaxyx(stdscr,h,w);
	for(int ii=0;ii<times;++ii)
	{
		if(stop_m)
			return;
		int gx;
		int gy;
		getyx(stdscr,gy,gx);
		int x;
		int y;
		get_pos(y,x);
		if(gy==y_top_margin_m&&yoff_m>0)
		{
			--yoff_m;
			refresh_m=true;
		}
		if(y>0)
		{
			move_pos(y-1,std::min(x,(int)lines_m[y-1].size()));
			draw_top_bar();
			draw_bottom_bar();
		}
	}
	cut_moved_m=true;
}

void editor_t::move_down(const int times)
{
	if(stop_m)
		return;
	int w;
	int h;
	getmaxyx(stdscr,h,w);
	for(int ii=0;ii<times;++ii)
	{
		if(stop_m)
			return;
		int gx;
		int gy;
		getyx(stdscr,gy,gx);
		int x;
		int y;
		get_pos(y,x);
		if(gy==h-y_bottom_margin_m-1&&y+1<(int)lines_m.size())
		{
			++yoff_m;
			refresh_m=true;
		}
		if(y+1<(int)lines_m.size())
		{
			int nx=std::min(x,(int)lines_m[y+1].size());
			if(nx<w)
				xoff_m=0;
			move_pos(y+1,nx);
			draw_top_bar();
			draw_bottom_bar();
		}
	}
	cut_moved_m=true;
}

void editor_t::newline()
{
	if(stop_m)
		return;
	int gx;
	int gy;
	getyx(stdscr,gy,gx);
	int x;
	int y;
	get_pos(y,x);
	std::string lhs=lines_m[y].substr(0,x);
	std::string rhs=lines_m[y].substr(x,lines_m[y].size());
	lines_m[y]=lhs;
	lines_m.insert(lines_m.begin()+y+1,rhs);
	int w;
	int h;
	getmaxyx(stdscr,h,w);
	if(gy==h-y_bottom_margin_m-1)
		++yoff_m;
	xoff_m=0;
	move_pos(y+1,0);
	refresh_m=true;
}

void editor_t::backspace()
{
	if(stop_m)
		return;
	int gx;
	int gy;
	getyx(stdscr,gy,gx);
	int x;
	int y;
	get_pos(y,x);
	int w;
	int h;
	getmaxyx(stdscr,h,w);
	if(gx==0&&xoff_m>0)
		--xoff_m;
	bool old_refresh=refresh_m;
	if(x>0)
	{
		lines_m[y].erase(x-1,1);
		move_pos(y,x-1);
		refresh_m=true;
	}
	else if(y>0)
	{
		if(gy==y_top_margin_m&&yoff_m>0)
		{
			--yoff_m;
			refresh_m=true;
		}
		std::string old_line=lines_m[y-1];
		lines_m[y-1]+=lines_m[y];
		lines_m.erase(lines_m.begin()+y);
		if((int)lines_m[y-1].size()>w)
			xoff_m=(int)lines_m[y-1].size()-w+1;
		else
			xoff_m=0;
		move_pos(y-1,old_line.size());
		refresh_m=true;
	}
	if(!old_refresh&&refresh_m)
		status_m=modified_str_m;
}

void editor_t::cut_line()
{
	if(stop_m)
		return;
	if(cut_moved_m)
	{
		cut_moved_m=false;
		cut_buffer_m.clear();
	}
	int x;
	int y;
	get_pos(y,x);
	home();
	size_t old_cut_buffer_size=cut_buffer_m.size();
	if(lines_m.size()==(size_t)y+1)
	{
		if(lines_m[y].size()>0)
			cut_buffer_m.push_back(lines_m[y]);
		lines_m[y]="";
	}
	else
	{
		cut_buffer_m.push_back(lines_m[y]);
		lines_m.erase(lines_m.begin()+y);
	}
	refresh_m=true;
	if(old_cut_buffer_size!=cut_buffer_m.size())
		status_m=modified_str_m;
}

void editor_t::uncut_lines()
{
	if(stop_m)
		return;
	for(size_t ii=0;ii<cut_buffer_m.size();++ii)
	{
		insert_string(cut_buffer_m[ii]);
		newline();
	}
	home();
	refresh_m=true;
}

void editor_t::del()
{
	if(stop_m)
		return;
	if(delch()!=ERR)
	{
		int x;
		int y;
		get_pos(y,x);
		bool old_refresh=refresh_m;
		if((int)lines_m[y].size()-x>0)
		{
			lines_m[y].erase(x,1);
			refresh_m=true;
		}
		else if(y+1<(int)lines_m.size())
		{
			std::string old_line=lines_m[y+1];
			lines_m.erase(lines_m.begin()+y+1);
			move_pos(y,lines_m[y].size());
			lines_m[y]+=old_line;
			refresh_m=true;
		}
		if(!old_refresh&&refresh_m)
			status_m=modified_str_m;
	}
}

void editor_t::insert_char(const char ch)
{
	if(stop_m)
		return;
	if(ch=='\t')
	{
		for(int ii=0;ii<4;++ii)
			insert_char(' ');
		return;
	}
	int x;
	int y;
	get_pos(y,x);
	int gx;
	int gy;
	getyx(stdscr,gy,gx);
	if(lines_m.size()<=0)
		lines_m.push_back("");
	lines_m[y].insert(x,1,ch);
	int w;
	int h;
	getmaxyx(stdscr,h,w);
	if(gx==w-1&&x+1<=(int)lines_m[y].size())
		++xoff_m;
	move_pos(y,x+1);
	refresh_m=true;
	status_m=modified_str_m;
}

void editor_t::insert_string(const std::string& str)
{
	for(size_t ii=0;ii<str.size();++ii)
		insert_char(str[ii]);
}

void editor_t::draw_top_bar()
{
	if(stop_m)
		return;
	int x;
	int y;
	get_pos(y,x);
	int gx;
	int gy;
	getyx(stdscr,gy,gx);
	int w;
	int h;
	getmaxyx(stdscr,h,w);
	move(0,0);
	clrtoeol();
	attron(A_REVERSE);
	std::string title="enano";
	std::string filename=name_m;
	if(status_m.size()>0)
		filename+=" ("+status_m+")";
	std::string line="line: "+std::to_string(y+1)+"/"+std::to_string(std::max((size_t)1,lines_m.size()));
	std::string pos=" pos: "+std::to_string(gx+xoff_m)+" ";
	int spaces=title.size()+filename.size()+line.size()+pos.size();
	if(w>spaces)
		spaces=w-spaces;
	else
		spaces=0;
	std::string blanks="";
	for(int ii=0;ii<spaces/2;++ii)
		blanks+=' ';
	if(spaces%2!=0)
		pos+=' ';
	type_string(title+blanks+filename+blanks+line+pos);
	attroff(A_REVERSE);
	move(gy,gx);
	refresh();
}

void editor_t::draw_bottom_bar()
{
	if(stop_m)
		return;
	int x;
	int y;
	get_pos(y,x);
	int gx;
	int gy;
	getyx(stdscr,gy,gx);
	int w;
	int h;
	getmaxyx(stdscr,h,w);
	for(int yy=h-y_bottom_margin_m;yy<h;++yy)
	{
		move(yy,0);
		clrtoeol();
	}
	move(h-1,0);
	std::vector<std::string> controls=
	{
		"^O Save",
		"^C Quit",
		"^K Cut Text",
		"^U Uncut Text"
	};
	for(auto control:controls)
	{
		attron(A_REVERSE);
		type_string(control.substr(0,2));
		attroff(A_REVERSE);
		type_string(control.substr(2,control.size()-2)+" ");
	}
	move(gy,gx);
	refresh();
}

void editor_t::resize()
{
	if(stop_m)
		return;
	int gx;
	int gy;
	getyx(stdscr,gy,gx);
	int w;
	int h;
	getmaxyx(stdscr,h,w);
	if(max_y()<=0)
		throw std::runtime_error("Window size too small.");
	erase();
	for(int yy=yoff_m;yy<(int)lines_m.size()&&yy<=yoff_m+h;++yy)
	{
		move(y_top_margin_m+yy-yoff_m,0);
		if(gy==y_top_margin_m+yy-yoff_m)
		{
			type_string(lines_m[yy].substr(xoff_m,w));
			if(xoff_m>0)
			{
				move(y_top_margin_m+yy-yoff_m,0);
				type_string("$");
			}
		}
		else
			type_string(lines_m[yy].substr(0,w));
	}
	if(gy>h-y_bottom_margin_m-1)
		gy=h-y_bottom_margin_m-1;
	if(gy<y_top_margin_m)
		gy=y_top_margin_m;
	move(gy,gx);
	draw_top_bar();
	draw_bottom_bar();
}

void editor_t::get_pos(int& y,int& x)
{
	if(stop_m)
		return;
	getyx(stdscr,y,x);
	if(y>0)
		y-=y_top_margin_m;
	x+=xoff_m;
	y+=yoff_m;
}

void editor_t::move_pos(const int y,const int x)
{
	if(stop_m)
		return;
	move(y_top_margin_m+y-yoff_m,x-xoff_m);
}

int editor_t::max_y()
{
	int w;
	int h;
	getmaxyx(stdscr,h,w);
	h-=y_top_margin_m+y_bottom_margin_m;
	return h;
}
