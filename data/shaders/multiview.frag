uniform sampler2D texture2;
uniform sampler2D texture3;

uniform sampler2D texture6;
uniform sampler2D texture7;


uniform float width;
uniform float height;

in vec2 uv;
out vec4 FragColor;

#if 1

void main()
{
	FragColor = texture2D(texture2, uv);
}

#else

void main()
{
	vec2 tx_coord;
	vec4 col2, col3, col6, col7, res;

	tx_coord = uv;


	//simple test on coordinates: get integer (pixel) position, compute their modulo 8. If same modulo draw black
	//this will results in oblique black lines every 8 pixels, with a slope of 1/1 = 45 degree
	//!! y=0 is bottom of the screen, x=0 is left

	int x = int(gl_FragCoord.x + 0.5);
	int y = int(gl_FragCoord.y + 0.5);

	res = vec4(0.0,0.0,0.0,0.0);

	int moduloy = y/8;
	moduloy = y - moduloy*8;

	int modulox = x/8;
	modulox = x - modulox*8;

//definition des composantes RGB

	int view_r = (3*x+y);
	int mod_r = view_r/8;
	mod_r = view_r - mod_r*8;


//choix des vues pour chaque pixel
	if (mod_r == 0) {
		col2 = texture2D(texture2, tx_coord);
		res.b = col2.b;
	}

	if (mod_r == 1){
		col2 = texture2D(texture2, tx_coord);
		col3 = texture2D(texture3, tx_coord);

		res.g = col2.g;
		res.b = col3.b;
	}

	if(mod_r == 2){
	col2 = texture2D(texture2, tx_coord);
	col3 = texture2D(texture3, tx_coord);

	res.r = col2.r;
	res.g = col3.g;
	}

	if(mod_r == 3){
		col3 = texture2D(texture3, tx_coord);
		res.r = col3.r;
	}

	if(mod_r == 4){
		col6 = texture2D(texture6, tx_coord);
		res.b = col6.b;
	}

	if(mod_r == 5){
		col6 = texture2D(texture6, tx_coord);
		col7 = texture2D(texture7, tx_coord);
		res.g = col6.g;
		res.b = col7.b;
	}

	if(mod_r == 6){
		col6 = texture2D(texture6, tx_coord);
		col7 = texture2D(texture7, tx_coord);
		res.r = col6.r;
		res.g = col7.g;
	}

	if(mod_r == 7){
		col7 = texture2D(texture7, tx_coord);
		res.r = col7.r;
	}



	gl_FragColor = res;

}

#endif
