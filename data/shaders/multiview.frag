uniform sampler2D texture_j1g;
uniform sampler2D texture_j1d;
uniform sampler2D texture_j1c;

uniform sampler2D texture_j2g;
uniform sampler2D texture_j2d;
uniform sampler2D texture_j2c;

uniform int nb_views;

uniform float width;
uniform float height;

in vec2 uv;
out vec4 FragColor;


void main()
{
	vec2 tx_coord;
	vec4 col2, col3, col4, col6, col7, col8, res;

	tx_coord = vec2(0.5*(1.+uv.s), 0.5*(1.0-uv.t));



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


if (nb_views == 3){
//choix des vues pour chaque pixel
	if (mod_r == 0) {
		col2 = texture2D(texture_j1g, tx_coord);
		res.b = col2.b;
		col8 = texture2D(texture_j2d, tx_coord);
		res.r = col8.r;
	}

	if (mod_r == 1){
		col2 = texture2D(texture_j1g, tx_coord);
		col3 = texture2D(texture_j1c, tx_coord);
		res.g = col2.g;
		res.b = col3.b;
	}

	if(mod_r == 2){
		col2 = texture2D(texture_j1g, tx_coord);
		res.r = col2.r;
		col3 = texture2D(texture_j1c, tx_coord);
		res.g = col3.g;
		col4 = texture2D(texture_j1d, tx_coord);
		res.b = col4.b;
	}

	if(mod_r == 3){
		col3 = texture2D(texture_j1c, tx_coord);
		res.r = col3.r;
		col4 = texture2D(texture_j1d, tx_coord);
		res.g = col4.g;
	}

	if(mod_r == 4){
	  col4 = texture2D(texture_j1d, tx_coord);
	  res.r = col4.r;
		col6 = texture2D(texture_j2g, tx_coord);
		res.b = col6.b;
	}

	if(mod_r == 5){
		col6 = texture2D(texture_j2g, tx_coord);
		res.g = col6.g;
		col7 = texture2D(texture_j2c, tx_coord);
		res.b = col7.b;
	}

	if(mod_r == 6){
		col6 = texture2D(texture_j2g, tx_coord);
		col7 = texture2D(texture_j2c, tx_coord);
		col8 = texture2D(texture_j2d, tx_coord);
		res.r = col6.r;
		res.g = col7.g;
		res.b = col8.b;
	}

	if(mod_r == 7){
		col7 = texture2D(texture_j2c, tx_coord);
		res.r = col7.r;
		col8 = texture2D(texture_j2d, tx_coord);
		res.g = col8.g;
	}
	}

	else if (nb_views==2){
	//choix des vues pour chaque pixel
		if (mod_r == 0) {
			col7 = texture2D(texture_j2d, tx_coord);
			res.r = col7.r;
		}

		if (mod_r == 1){
			col2 = texture2D(texture_j1g, tx_coord);
			res.b = col2.b;
		}

		if(mod_r == 2){
			col2 = texture2D(texture_j1g, tx_coord);
			col3 = texture2D(texture_j1d, tx_coord);

			res.b = col3.b;
			res.g = col2.g;
		}

		if(mod_r == 3){
			col2 = texture2D(texture_j1g, tx_coord);
			col3 = texture2D(texture_j1d, tx_coord);

			res.g = col3.g;
			res.r = col2.r;
		}

		if(mod_r == 4){
			col3 = texture2D(texture_j1d, tx_coord);
			res.r = col3.r;
		}

		if(mod_r == 5){
			col6 = texture2D(texture_j2g, tx_coord);
			res.b = col6.b;
		}

		if(mod_r == 6){
			col6 = texture2D(texture_j2g, tx_coord);
			col7 = texture2D(ev, tx_coord);
			res.b = col7.b;
			res.g = col6.g;
		}

		if(mod_r == 7){
			col6 = texture2D(texture_j2g, tx_coord);
			col7 = texture2D(texture_j2d, tx_coord);
			res.g = col7.g;
			res.r = col6.r;
		}




	}


	FragColor = res;

}
