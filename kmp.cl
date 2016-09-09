__kernel void getPi(__global char* tmp_pattern, __global int* tmp_pi, int pattern_size){
	int i = 0, j = 1;
	int gid = get_global_id(0);
	
	/*size�� �޾Ƽ� ó���ϰ� �� ��*/
	tmp_pi[gid*pattern_size] = -1;
	
	while(j < pattern_size){
		if(tmp_pattern[gid*pattern_size+j] == tmp_pattern[gid*pattern_size+i]){
			tmp_pi[gid*pattern_size+j] = ++i;
			j++;
		}

		else{
			tmp_pi[gid*pattern_size+j] = 0;
			i = 0;
			j++;
		}
	}

}

/*pattern, string, pi�� �Է¹ް� ��ġ�� ������ ���*/

__kernel void kmp(__global char* tmp_pattern, __global char* tmp_string, __global int* tmp_pi, __global int* ret, int pattern_size, int string_size){
	int gid = get_global_id(0);
	int i, j = 0;
	//ret[gid] = 0;

	for(i = 0; i < string_size; i++){
		while(tmp_string[i] != tmp_pattern[gid*pattern_size+j] && j > 0){
			j = tmp_pi[gid*pattern_size+j-1];
		}
		if(tmp_string[i] == tmp_pattern[gid*5+j]){
			if(j == pattern_size-1){
				ret[gid]++;
				j = tmp_pi[gid*5+j];
			}
			else{
				j++;
			}
		}
	}
}

/*gid������ ����� ���� ó�����ִ� if���� �߰��ؾ���*/