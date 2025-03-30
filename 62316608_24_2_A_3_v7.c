#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>

#define N4_INDEX 25 //Nmax = (4 ^ (2 ^ N4_INDEX) - 1) / 3  に設定する

#define NTT_NUM 3892314113 //29*(2^27)+1
#define PRI_ROOT 3 //NTT_NUMの原始根
#define INDEX_MAX 134217728 //2^27
#define INDEX_MAX_ORDER 27 //2^27
#define NOT_KARA_LAST 27 //カラツバ法を実行しない最大のiを指定 27以下を推奨 大きいほど計算時間が短縮

#define I_TEXT "00_i_v7.txt"
#define K_TEXT "00_k_v7.txt"
#define NMAX_TEXT "00_Nmax_v7.txt"
#define EXE_PRINT 0 //0:プリントを実行しない 1:プリントを実行する

#define MAX_THREAD 12 //最大スレッド数 9以上を推奨

void print_list(char *input_point);
char *ntt_equ_rec(unsigned int *test_lst2, unsigned int index_max_order, char *input_point_1, unsigned long long branch_num);

unsigned int cal_num;
double cal_pro;

unsigned int now_thread = 1; //現在稼働中のスレッド数
pthread_mutex_t	mutex, mutex2;
pthread_t thread_lst[MAX_THREAD-1];
char thread_exe_lst[MAX_THREAD-1] = {0};

struct thread_num{
	char *return_point; //戻り値の格納先

	unsigned int *test_lst2;
	unsigned int index_max_order;
	char *input_point_1;
	unsigned long long branch_num;
};

//max関数
unsigned long long my_max(unsigned long long a, unsigned long long b){
	if(a < b){
		return b;
	}
	return a;
}

//min関数
unsigned long long my_min(unsigned long long a, unsigned long long b){
	if(a < b){
		return a;
	}
	return b;
}

//桁数を返す関数(char*用) (末尾の0も桁数に含めることに注意 例 3 2 4 0 0 -1 -> 5)
unsigned long long dig_char_ll(char *input_point){
	unsigned long long i = 0;
	for(;;i++){
		if(input_point[i] == -1){
			return i;
		}
	}
}

//桁数を返す関数(数字用)
unsigned int dig_num(unsigned long long input_num){
	unsigned long long m = 1;
	unsigned int i = 0;
	for(;m<=input_num;i++){
		m *= 10;
	}
	return i;
}

//配列を値に変換する関数(char *-> unsigned long long)
unsigned long long cov_num(char *input_point){
	unsigned long long m = 1, num = 0;
	for(unsigned int i=0;input_point[i] != -1;i++){
		num += input_point[i] * m;
		m *= 10;
	}
	return num;
}

//値を配列に変換する関数(int -> char *)
char *cov_list(unsigned long long input_num){
	char *return_point;
	unsigned int max_dig = dig_num(input_num)+1;
	unsigned long long m = 1;

    return_point = (char *)malloc((sizeof(char)) * max_dig);
    if(return_point == NULL){
        printf("memory error1\n");
        exit(1);
    }

    for(unsigned int i=0;i<max_dig-1;i++){
        return_point[i] = (input_num % (m * 10)) / m;
		m *= 10;
    }
	return_point[max_dig-1] = -1;

    return return_point;
}

//配列のコピーを作成
char *copy_list(char *parent){
	char *child = NULL;
	unsigned long long max_dig = dig_char_ll(parent) + 1;
	child = (char *)malloc((sizeof(char))*max_dig);
	if(child == NULL){
        printf("memory error2\n");
        exit(1);
    }

    for(unsigned long long i=0;i<max_dig;i++){
        child[i] = parent[i];
    }

	return child;
}

//リストを分割してコピーしてreturn，digで桁数指定，left_rightが0なら小さい側，1なら大きい側
char *half_list(char *parent, unsigned long long dig, unsigned int left_right){
	char *child = NULL;
	unsigned long long max_dig;
	if(left_right == 0){
		max_dig = dig + 1;
	}
	else {
		max_dig = dig_char_ll(parent) - dig + 1;
	}

	if(max_dig <= 0)
	{
		max_dig = 1;
	}

	child = (char *)malloc((sizeof(char))*max_dig);
	if(child == NULL){
        printf("memory error3, dig=%lld, left_right=%d, cov_num(parent)=%lld, max_dig=%lld\n", dig, left_right, cov_num(parent), max_dig);
        exit(1);
    }

	for(unsigned long long i=0;i<max_dig-1;i++){
		if(left_right == 0){
			child[i] = parent[i];
		}
		else {
			child[i] = parent[i+dig];
		}        
    }
	child[max_dig-1] = -1;

	return child;
}

//桁数拡張 (例: 4 2 3 -1 -> 4 2 3 0 0 -1)
char *dig_exp_ll(char *input_point, unsigned long long dig_after){
	unsigned long long dig_before = dig_char_ll(input_point);
	input_point = (char *)realloc(input_point, sizeof(char) * (dig_after + 1));
	if(input_point == NULL){
        printf("memory error4\n");
        exit(1);
    }

	for(unsigned long long i=dig_before; i<dig_after; i++){
		input_point[i] = 0;
	}
	input_point[dig_after] = -1;

	return input_point;
}

//桁数削減 (例: 4 2 3 0 0 -1 -> 4 2 3 -1)
char *dig_red(char **input_point_point){
	char *input_point = *input_point_point;
	unsigned long long dig_before = dig_char_ll(input_point);

	for(unsigned long long i=dig_before; i > 0; i--){
		if(input_point[i-1] == 0){
			input_point[i-1] = -1;
		}
		else{
			break;
		}
	}

	unsigned long long max_dig = dig_char_ll(input_point) + 1;

	char *output_point = NULL;
	output_point = (char *)malloc((sizeof(char))*max_dig);
	if(output_point == NULL){
        printf("memory error5\n");
        exit(1);
    }

	for(unsigned long long i=0;i<max_dig;i++){
		output_point[i] = input_point[i];
	}

	free(input_point);

	*input_point_point = output_point;

	return output_point;
}

//左シフト (例: 4 2 3 -1 -> 0 0 4 2 3 -1)
char *left_shift(char *input_point, unsigned long long dig_shift){
	unsigned long long dig_before = dig_char_ll(input_point);
	input_point = (char *)realloc(input_point, sizeof(char) * (dig_before + dig_shift + 1));
	if(input_point == NULL){
        printf("memory error6\n");
        exit(1);
    }

	input_point[dig_before+dig_shift] = -1;
	for(unsigned long long i=dig_before + dig_shift; i > dig_shift; i--){
		input_point[i-1] = input_point[i-dig_shift-1];
	}
	for(unsigned long long i=0; i<dig_shift; i++){
		input_point[i] = 0;
	}
	
	return input_point;
}

//左シフト(改良版) (例: 4 2 3 -1 -> 0 0 4 2 3 -1)
char *left_shift_adv(char *input_point, unsigned long long dig_shift){
	unsigned long long dig_before = dig_char_ll(input_point);
	char *return_point = (char *)malloc(sizeof(char) * (dig_before + dig_shift + 1));
	if(return_point == NULL){
        printf("memory error6\n");
        exit(1);
    }

	return_point[dig_before+dig_shift] = -1;
	for(unsigned long long i=0; i<dig_before+dig_shift; i++){
		if(i < dig_shift){
			return_point[i] = 0;
		}
		else{
			return_point[i] = input_point[i-dig_shift];
		}
	}
	
	return return_point;
}

//配列内容をprint(デバッグ用)
void print_list(char *input_point){
	for(unsigned int i=0;;i++){
		printf("%d ",input_point[i]);
		if(input_point[i] == -1){
			break;
		}	
	}
	printf("\n");
}

//配列内容をprint(デバッグ用)
void print_list_int(unsigned int *input_point){
	for(unsigned int i=0;;i++){
		printf("%u ",input_point[i]);
		if(input_point[i] == -1){
			break;
		}	
	}
	printf("\n");
}

//配列内容を数字としてprint(デバッグ用)
void print_list_num(char *input_point){
	for(unsigned long long i = dig_char_ll(input_point); i > 0; i--){
		printf("%d",input_point[i-1]);
	}
	printf("\n");
}

//桁数を指定して配列内容を数字としてprint(デバッグ用)
void print_list_num_des(char *input_point, unsigned long long num){
	unsigned int count = 2;
	for(unsigned long long i=dig_char_ll(input_point);i>=dig_char_ll(input_point)-num+1;i--){
		printf("%d",input_point[i-1]);
		count++;
		if(count >= 3){
			count -=3;
			printf(" ");
		}
	}
	printf("\n");
}

//桁数を指定して配列内容の最後部分を数字としてprint(デバッグ用)
void print_list_num_last(char *input_point, long long num){
	for(long long i=num-1;i>=0;i--){
		printf("%d",input_point[i]);
	}
	printf("\n");
}

//足し算
char *add_list(char *input_point_1, char *input_point_2){
	unsigned long long input_dig_1 = dig_char_ll(input_point_1), input_dig_2 = dig_char_ll(input_point_2); //受け取った引数ポインタの桁数
	unsigned long long output_dig = my_max(input_dig_1,input_dig_2) + 1; //送り込むポインタの桁数

	char *child = NULL;
	child = (char *)malloc((sizeof(char))*(output_dig+1));
	if(child == NULL){
        printf("memory error7\n");
        exit(1);
    }
	for(unsigned long long i=0;i<output_dig;i++){
		child[i] = 0;
		if(i < input_dig_1){
			child[i] += input_point_1[i];
		}
		if(i < input_dig_2){
			child[i] += input_point_2[i];
		}
	}
	for(unsigned long long i=0;i<output_dig;i++){
		child[i+1] += child[i] / 10;
		child[i] = child[i] % 10;
	}
	child[output_dig] = -1;

	return dig_red(&child);
}

//引き算
char *sub_list(char *input_point_1, char *input_point_2){
	unsigned long long input_dig_1 = dig_char_ll(input_point_1), input_dig_2 = dig_char_ll(input_point_2); //受け取った引数ポインタの桁数
	unsigned long long output_dig = input_dig_1; //送り込むポインタの桁数

	char *child = NULL;
	child = (char *)malloc((sizeof(char))*(output_dig+1));
	if(child == NULL){
        printf("memory error8\n");
        exit(1);
    }
	for(unsigned long long i=0;i<output_dig;i++){
		child[i] = 0;
		if(i < input_dig_1){
			child[i] += input_point_1[i];
		}
		if(i < input_dig_2){
			child[i] -= input_point_2[i];
		}
	}
	for(unsigned long long i=0;i<output_dig;i++){
		if(child[i] < 0){
			child[i] += 10;
			child[i+1] -= 1;
		}
	}
	child[output_dig] = -1;

	return dig_red(&child);
}

//NTTリストの作成 (ntt_num: 素数mod，pri_root: ntt_numの原始根，index_max: 配列サイズ(必ず(ntt_num-1)の約数でなければならない))
unsigned int *ntt_make_adv(unsigned long long ntt_num, unsigned long long pri_root, unsigned long long index_max){
	printf("\nntt make start\n");
	unsigned long long ntt_const = (ntt_num - 1) / index_max, pri_true = 1, index_max2 = index_max / 100, count_2 = 0;
    unsigned int *test_lst2 = (unsigned int *)malloc(sizeof(unsigned int) * (index_max+1));
    if(test_lst2 == NULL){
        printf("ntt_make_adv null point 1st error\n");
        exit(1);
    }

	for(unsigned long long i = 0; i < ntt_const; i++){
		pri_true = (pri_true * pri_root) % ntt_num;
	}

	test_lst2[0] = 1;

    for(unsigned long long i = 1; i <= index_max; i++){
        test_lst2[i] =  (test_lst2[i-1] * pri_true) % ntt_num;
        if(i >= index_max2 * count_2){
            printf("NTT_LIST: %lld%%\n",count_2);
			count_2 += 20;
        }
    }

	if(test_lst2[index_max] != 1){
		printf("ntt_make_adv numeric error: %lld\n", test_lst2[index_max]);
	}

	printf("ntt make finish\n");

    return test_lst2;
}

//2のn乗を返す関数 int index_order:指数
unsigned long long pow_2(unsigned int index_order){
	unsigned long long num = 1;
	for(unsigned int i=0;i<index_order;i++){
		num *= 2;
	}
	return num;
}

//引数以上のうち最小の2のn乗が存在するときnを返す関数 unsigned long long input_num:引数
unsigned int min_index_2_ll(unsigned long long input_num){
	unsigned int return_num = 0;
	unsigned long long pow_num = 1;

	while(pow_num < input_num){
		return_num++;
		pow_num *= 2;
	}
	return return_num;
}

//ntt実行関数 index_order:配列サイズのlog2
unsigned int *ntt_exe_adv(unsigned int *test_lst2, char *input_point_1, unsigned int index_order, unsigned long long index_now, unsigned long long my_number){
	if(index_order == 0){
		unsigned int *return_point;
		return_point = (unsigned int *)malloc(sizeof(unsigned int) * 2);
		if(return_point == NULL){
			printf("ntt_exe_adv null point 1st error\n");
			exit(1);
		}
		return_point[0] = (unsigned int)(input_point_1[my_number]);
		return_point[1] = -1;
		return return_point;
	}

	unsigned int *ntt_point;
	unsigned long long j;
	unsigned long long input_dig = pow_2(index_order);

	ntt_point = (unsigned int *)malloc(sizeof(unsigned int) * (input_dig + 1));
	if(ntt_point == NULL){
		printf("ntt_exe_adv null point 2nd error\n");
		exit(1);
	}
	ntt_point[input_dig] = -1;

	unsigned int *ntt_half_point_1, *ntt_half_point_2;

	ntt_half_point_1 = ntt_exe_adv(test_lst2, input_point_1, index_order-1, index_now*2, my_number);
	ntt_half_point_2 = ntt_exe_adv(test_lst2, input_point_1, index_order-1, index_now*2, my_number+index_now);
	

	unsigned long long m, k = pow_2(INDEX_MAX_ORDER - index_order), k2;

	for(unsigned long long i=0;i<input_dig;i++){
		j = i % (input_dig / 2);
		k2 = k * i;
		m = ((unsigned long long)(ntt_half_point_1[j])) + ((unsigned long long)(ntt_half_point_2[j])) * ((unsigned long long)(test_lst2[k2]));
		ntt_point[i] = (unsigned int)(m % ((unsigned long long)NTT_NUM));
	}

	free(ntt_half_point_1);
	free(ntt_half_point_2);

	return ntt_point;
}

//intt実行関数 index_order:配列サイズのlog2
unsigned int *intt_exe_adv(unsigned int *test_lst2, unsigned int *input_point_1, unsigned int index_order, unsigned long long index_now, unsigned long long my_number){
	if(index_order == 0){
		unsigned int *return_point;
		return_point = (unsigned int *)malloc(sizeof(unsigned int) * 2);
		if(return_point == NULL){
			printf("intt_exe_adv null point 1st error\n");
			exit(1);
		}
		return_point[0] = (unsigned int)(input_point_1[my_number]);
		return_point[1] = -1;
		return return_point;
	}

	unsigned int *ntt_point;
	unsigned long long j;
	unsigned long long input_dig = pow_2(index_order);

	ntt_point = (unsigned int *)malloc(sizeof(unsigned int) * (input_dig + 1));
	if(ntt_point == NULL){
		printf("intt_exe_adv null point 2nd error\n");
		exit(1);
	}
	ntt_point[input_dig] = -1;

	unsigned int *ntt_half_point_1, *ntt_half_point_2;

	ntt_half_point_1 = intt_exe_adv(test_lst2, input_point_1, index_order-1, index_now*2, my_number);
	ntt_half_point_2 = intt_exe_adv(test_lst2, input_point_1, index_order-1, index_now*2, my_number+index_now);

	unsigned long long m, k = pow_2(INDEX_MAX_ORDER - index_order), k2, k3 = pow_2(index_order);

	for(unsigned int i=0;i<input_dig;i++){
		j = i % (input_dig / 2);
		k2 = (INDEX_MAX - k * i) % INDEX_MAX;
		m = ((unsigned long long)(ntt_half_point_1[j])) + ((unsigned long long)(ntt_half_point_2[j])) * ((unsigned long long)(test_lst2[k2]));
		ntt_point[i] = (unsigned int)(m % ((unsigned long long)NTT_NUM));
	}

	free(ntt_half_point_1);
	free(ntt_half_point_2);

	return ntt_point;
}

//2進数に変換しリスト形式でreturnする関数
char *binary_lst(unsigned long long input_num){
	unsigned int i_max = min_index_2_ll(input_num+1);

	char *output_lst = (char *)malloc(sizeof(char) * (i_max + 1));
	if(output_lst == NULL){
		printf("binary_lst null point 1st error\n");
		exit(1);
	}

	unsigned long long num = 1;

	for(unsigned int i=0;i<i_max;i++){
		output_lst[i] = (char)(((unsigned long long)input_num % (num*2)) / num);
		
		input_num -= (unsigned int)((unsigned long long)input_num % (num*2));
		
		num *= 2;		
	}
	output_lst[i_max] = -1;

	return output_lst;
}

//nttの実行 return input_lstの2乗
char *ntt_equ(unsigned int *test_lst2, char *input_point_1){
	unsigned long long input_dig_1 = dig_char_ll(input_point_1);

	if(input_dig_1 == 0){ //数の桁数が0なら(=どちらかの数が0なら)，0をreturnする
		free(input_point_1);
		return cov_list(0);
	}

	if(input_dig_1 <= 9){ //値が小さいなら普通に掛け算
		unsigned long long little_num_1 = cov_num(input_point_1);
		unsigned long long little_num = little_num_1 * little_num_1;
		free(input_point_1);
		return cov_list(little_num);
	}

	unsigned int input_point_index_2 = min_index_2_ll(dig_char_ll(input_point_1)) + 1;
	unsigned long long input_point_pow_2 = pow_2(input_point_index_2);
	input_point_1 = dig_exp_ll(input_point_1, input_point_pow_2);  //桁数をいい感じに拡張

	unsigned int *ntt_input_point_1 = ntt_exe_adv(test_lst2, input_point_1, input_point_index_2, 1, 0); //入力された数字をntt化
	free(input_point_1);

	unsigned int *ntt_input_point_2 = (unsigned int *)malloc(sizeof(unsigned int) * (input_point_pow_2+1));
	if(ntt_input_point_2 == NULL){
		printf("ntt_equ null point 1st error\n");
		exit(1);
	}

	unsigned long long each_di, each_di2, each_debug;

	for(unsigned long long i=0;i<=input_point_pow_2;i++){
		if(i < input_point_pow_2){
			each_di = (unsigned long long)(ntt_input_point_1[i]);
			ntt_input_point_2[i] = (unsigned int)((each_di * each_di) % NTT_NUM);
		}
		else{
			ntt_input_point_2[i] = -1;
		}
	}
	
	free(ntt_input_point_1);
	unsigned int *intt_input_point_1 = intt_exe_adv(test_lst2, ntt_input_point_2, input_point_index_2,1,0); //数字をintt化
	free(ntt_input_point_2);

	unsigned long long *intt_input_point_2 = (unsigned long long *)malloc(sizeof(unsigned long long) * (input_point_pow_2+1));
	if(intt_input_point_2 == NULL){
		printf("ntt_equ null point 2nd error\n");
		exit(1);
	}

	unsigned int dig_count = 0;
	intt_input_point_2[input_point_pow_2] = -1;
	for(unsigned long long i=0;i<input_point_pow_2;i++){
		each_di = (unsigned long long)(intt_input_point_1[i]);
		each_di2 = each_di % input_point_pow_2;

		if(each_di2 == 0){
			dig_count = 0;
		}
		else{
			dig_count = (unsigned int)(input_point_pow_2 - each_di2);
		}
		each_debug = each_di;
		each_di += (unsigned long long)NTT_NUM * (unsigned long long)dig_count;

		if(each_di % input_point_pow_2 == 0){
			intt_input_point_2[i] = each_di / input_point_pow_2;
		}
		else{
			printf("ntt_equ division error\n");
			printf("dig_count: %d\n",dig_count);
			printf("each_di: %lld\n",each_di);
			print_list_num(binary_lst(each_di));
			printf("each_debug: %lld\n",each_debug);
			print_list_num(binary_lst(each_debug));
			exit(1);
		}
	}
	intt_input_point_2[input_point_pow_2] = -1;	

	free(intt_input_point_1);

	char *intt_input_point_3 = (char *)malloc(sizeof(char) * (input_point_pow_2+1));
	if(intt_input_point_3 == NULL){
		printf("ntt_equ null point 3rd error\n");
		exit(1);
	}

	for(unsigned long long i=0;i<input_point_pow_2;i++){
		intt_input_point_2[i+1] += intt_input_point_2[i] / 10;
		intt_input_point_3[i] = intt_input_point_2[i] % 10;
	}
	intt_input_point_3[input_point_pow_2] = -1;

	free(intt_input_point_2);

	return intt_input_point_3;
}

//受け取ったリストが0を示すなら1，示さないなら0をreturnする関数
char zero_check(char *input_point){
	unsigned long long i = 0;
	while(input_point[i] != -1){
		if(input_point[i] != 0){
			return 0;
		}
		i++;
	}
	return 1;
}

//現在の進捗度を更新し，printする関数
void print_progress(unsigned long long branch_num){
	pthread_mutex_lock(&mutex2);
	cal_pro += 100.0 / branch_num;
	if(cal_pro >= 100){
		printf("number %d calculation progress: 100.000%%\n", cal_num);
	}
	else{
		printf("number %d calculation progress: %.3lf%%\n", cal_num, cal_pro);
	}
	fflush(stdout);
	pthread_mutex_unlock(&mutex2);
}

void *ntt_equ_thread(void *input_point){
	struct thread_num *thread_point = (struct thread_num *)input_point;

	thread_point->return_point = ntt_equ_rec(thread_point->test_lst2, thread_point->index_max_order, thread_point->input_point_1, thread_point->branch_num);
}

//nttの実行(再帰関数) return input_lst1 ** 2
char *ntt_equ_rec(unsigned int *test_lst2, unsigned int index_max_order, char *input_point_1, unsigned long long branch_num){
	if(zero_check(input_point_1)){ //再帰終了条件1
		print_progress(branch_num);
		return cov_list(0);
	}

	unsigned long long input_dig_1 = dig_char_ll(input_point_1);
	unsigned int input_point_index_2 = min_index_2_ll(input_dig_1) + 1;
	char *return_point;

	if(input_point_index_2 <= index_max_order){ //再帰終了条件2
		char *input_point_1_c = copy_list(input_point_1);
		return_point = ntt_equ(test_lst2, input_point_1_c); 
		dig_red(&return_point);
		print_progress(branch_num);
		return return_point;
	}

	unsigned int half_dig = (unsigned int)pow_2(input_point_index_2 - 2);

	char *my_point_1_small = half_list(input_point_1, half_dig, 0);
	char *my_point_1_big = half_list(input_point_1, half_dig, 1);
	char *my_point_1_total = add_list(my_point_1_small, my_point_1_big);

	char *child_big, *child_small;
	unsigned int thread_num_big, thread_num_small;
	char big_thread_exe = 0, small_thread_exe = 0;  //スレッド実行をしたか否か
	struct thread_num *thread_arg_big, *thread_arg_small;

	pthread_mutex_lock(&mutex);
	if(now_thread < MAX_THREAD){
		for(int i=0; i<MAX_THREAD-1; i++){
			if(thread_exe_lst[i] == 0){
				thread_exe_lst[i] = 1;
				thread_num_big = i;
				break;
			}
			else if(i == MAX_THREAD-2){
				printf("ntt_equ_rec thread_exe_lst 1st error\n");
				exit(1);
			}
		}
		now_thread++;
		pthread_mutex_unlock(&mutex);

		big_thread_exe = 1;

		thread_arg_big = (struct thread_num *)malloc(sizeof(struct thread_num));
		if(thread_arg_big == NULL){
			printf("ntt_equ_rec null point 1st error\n");
			exit(1);
		}

		thread_arg_big->return_point = NULL;
		thread_arg_big->test_lst2 = test_lst2;
		thread_arg_big->index_max_order = index_max_order;
		thread_arg_big->input_point_1 = my_point_1_big;
		thread_arg_big->branch_num = branch_num*3;

		if(pthread_create(&thread_lst[thread_num_big], NULL, &ntt_equ_thread, thread_arg_big) != 0){
			printf("ntt_equ_rec pthread create 1st error\n");
			exit(1);
		}
	}
	else{
		pthread_mutex_unlock(&mutex);
		big_thread_exe = 0;
		child_big = ntt_equ_rec(test_lst2, index_max_order, my_point_1_big, branch_num*3);
		free(my_point_1_big);	
	}

	pthread_mutex_lock(&mutex);
	if(now_thread < MAX_THREAD){
		for(int i=0; i<MAX_THREAD-1; i++){
			if(thread_exe_lst[i] == 0){
				thread_exe_lst[i] = 1;
				thread_num_small = i;
				break;
			}
			else if(i == MAX_THREAD-2){
				printf("ntt_equ_rec thread_exe_lst 2nd error\n");
				exit(1);
			}
		}
		now_thread++;
		pthread_mutex_unlock(&mutex);

		small_thread_exe = 1;

		thread_arg_small = (struct thread_num *)malloc(sizeof(struct thread_num));
		if(thread_arg_small == NULL){
			printf("ntt_equ_rec null point 2nd error\n");
			exit(1);
		}

		thread_arg_small->return_point = NULL;
		thread_arg_small->test_lst2 = test_lst2;
		thread_arg_small->index_max_order = index_max_order;
		thread_arg_small->input_point_1 = my_point_1_small;
		thread_arg_small->branch_num = branch_num*3;

		if(pthread_create(&thread_lst[thread_num_small], NULL, &ntt_equ_thread, thread_arg_small) != 0){
			printf("ntt_equ_rec pthread create 2nd error\n");
			exit(1);
		}
	}
	else{
		pthread_mutex_unlock(&mutex);
		small_thread_exe = 0;
		child_small = ntt_equ_rec(test_lst2, index_max_order, my_point_1_small, branch_num*3);
		free(my_point_1_small);
	}

	char *child_middle_1 = ntt_equ_rec(test_lst2, index_max_order, my_point_1_total, branch_num*3);
	free(my_point_1_total);

	if(big_thread_exe){
		pthread_join(thread_lst[thread_num_big], NULL);

		pthread_mutex_lock(&mutex);
		thread_exe_lst[thread_num_big] = 0;
		now_thread--;
		pthread_mutex_unlock(&mutex);

		free(my_point_1_big);

		child_big = thread_arg_big->return_point;
		free(thread_arg_big);
	}

	if(small_thread_exe){
		pthread_join(thread_lst[thread_num_small], NULL);

		pthread_mutex_lock(&mutex);
		thread_exe_lst[thread_num_small] = 0;
		now_thread--;
		pthread_mutex_unlock(&mutex);

		free(my_point_1_small);

		child_small = thread_arg_small->return_point;
		free(thread_arg_small);
	}

	char *child_middle_2 = sub_list(child_middle_1, child_small);
	free(child_middle_1);

	char *child_middle_3 = sub_list(child_middle_2, child_big);
	free(child_middle_2);

	child_big = left_shift(child_big, half_dig*2);
	child_middle_3 = left_shift(child_middle_3, half_dig);

	char *return_point_1 = add_list(child_big, child_middle_3);
	free(child_big);
	free(child_middle_3);

	char *return_point_2 = add_list(return_point_1, child_small);
	free(return_point_1);
	free(child_small);

	return return_point_2;
}

//受け取ったリストを元に，Nmax，k，iをfileに書き込む関数
void output_file(char *n4){
	printf("\nstart writing file\n");
	char *n4_lst2 = NULL, *n4_lst3 = NULL, *n4_lst4 = NULL; //Nmax, k, i用
	unsigned long long max_di_true = dig_char_ll(n4);

	n4_lst2 = (char*)malloc(sizeof(char) * max_di_true);
	n4_lst3 = (char*)malloc(sizeof(char) * max_di_true);
	n4_lst4 = (char*)malloc(sizeof(char) * max_di_true);
	if(n4_lst3 == NULL || n4_lst4 == NULL || n4_lst2 == NULL)
	{
		printf("output_file null point error 1st\n");
		exit(0);
	}

	n4[0] -= 1;
	unsigned long long n4_di,n4_di_4,n4_di_5;
	char remain=0;

	if(n4[max_di_true-1] >= 3)
	{
		n4_di = max_di_true;
	}
	else
	{
		n4_di = max_di_true - 1;
	}

	if(n4[max_di_true-1] >= 2)
	{
		n4_di_4 = max_di_true;
	}
	else
	{
		n4_di_4 = max_di_true - 1;
	}

	for(unsigned long long i=max_di_true;i>0;i--)
	{
		n4_lst3[i-1] = (unsigned int)(remain * 10 + n4[i-1]) / 3;
		remain = (remain * 10 + n4[i-1]) % 3; //k
	}

	n4[0] -= 1;
	remain = 0;

	for(unsigned long long i=max_di_true;i>0;i--)
	{
		n4_lst2[i-1] = (remain * 10 + n4[i-1]) / 2;
		remain = (remain * 10 + n4[i-1]) % 2; //Nmax
	}

	n4_lst3[0] -= 1; //k -= 1
	remain = 0;

	if(n4_lst3[n4_di-1] >= 2)
	{
		n4_di_5 = n4_di;
	}
	else
	{
		n4_di_5 = n4_di - 1;
	}

	for(unsigned long long i=n4_di;i>0;i--)
	{
		n4_lst4[i-1] = (remain * 10 + n4_lst3[i-1]) / 2;
				
		remain = (remain * 10 + n4_lst3[i-1]) % 2; //i
	}
	n4_lst4[0] += 1;

	for(unsigned long long i=0;;i++)
	{
		if(n4_lst4[i] < 10)
		{
			break;
		}
		n4_lst4[i+1] += n4_lst4[i] / 10;
		n4_lst4[i] = n4_lst4[i] % 10;
	}

	n4_lst3[0] += 1; //k += 1

	FILE *Nmax_FILE, *k_FILE, *i_FILE;

	char Nmax_FILE_name[] = NMAX_TEXT;
	char k_FILE_name[] = K_TEXT;
	char i_FILE_name[] = I_TEXT;

	unsigned int num = N4_INDEX;

	Nmax_FILE_name[0] = (num / 10) + 48;
	Nmax_FILE_name[1] = (num % 10) + 48;
	k_FILE_name[0] = (num / 10) + 48;
	k_FILE_name[1] = (num % 10) + 48;
	i_FILE_name[0] = (num / 10) + 48;
	i_FILE_name[1] = (num % 10) + 48;

	Nmax_FILE = fopen(Nmax_FILE_name,"w");
	k_FILE = fopen(k_FILE_name,"w");
	i_FILE = fopen(i_FILE_name,"w");

	if(Nmax_FILE == NULL || k_FILE == NULL || i_FILE == NULL){
		printf("output_file fopen error\n");
		exit(1);
	}

	for(unsigned long long i=n4_di;i>0;i--)
	{
		fprintf(k_FILE,"%d",n4_lst3[i-1]);
		
	}
	fprintf(k_FILE,", 1");
	printf("   k_file writing completed\n");

	for(unsigned long long i=n4_di_4;i>0;i--)
	{
		fprintf(Nmax_FILE,"%d",n4_lst2[i-1]);
		
	}
	printf("Nmax_file writing completed\n");

	for(unsigned long long i=n4_di_5;i>0;i--)
	{
		fprintf(i_FILE,"%d",n4_lst4[i-1]);
		
	}
	printf("   i_file writing completed\n");

	free(n4_lst2);
	free(n4_lst3);
	free(n4_lst4);

	fclose(Nmax_FILE);
	fclose(k_FILE);
	fclose(i_FILE);

	printf("all files have been written\n\n");
}

//受け取ったリストのなかに，各数字が何個入っているかを数え上げ，printする関数
void lst_check(char *input_lst){
	unsigned long long count_lst[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	while(*input_lst != -1){
		if(*input_lst >= 0 && *input_lst <= 9){
			(count_lst[*input_lst])++;
		}
		input_lst++;
	}

	for(unsigned int i=0;i<10;i++){
		printf("%d appeared %lld times\n", i, count_lst[i]);
	}
}

//経過時間をreturnする関数
float time_fun(struct timeval t1){
	struct timeval t2;
	struct timezone dummy;
	gettimeofday(&t2, &dummy);

	return (t2.tv_sec - t1.tv_sec) + (float)(t2.tv_usec - t1.tv_usec)/1000000;
}

int main()
{
	struct timeval start_true_time, start_lap_time;
	struct timezone dummy;
	gettimeofday(&start_true_time, &dummy);
	pthread_mutex_init(&mutex, NULL);
	pthread_mutex_init(&mutex2, NULL);

	unsigned int *test_lst2 = ntt_make_adv(NTT_NUM, PRI_ROOT, INDEX_MAX);
	printf("ntt make time: %.2fs\n\n", time_fun(start_true_time));

	char *main_num = cov_list(4), *main_num2;

	for(unsigned int i=0;i<N4_INDEX;i++){
		cal_num = i + 1;
		cal_pro = 0.0;	
		gettimeofday(&start_lap_time, &dummy);

		pthread_mutex_lock(&mutex);
		now_thread = 1;
		pthread_mutex_unlock(&mutex);

		printf("now calculation i = %d\n", i+1);

		main_num2 = ntt_equ_rec(test_lst2, my_max(my_min(NOT_KARA_LAST, i-1), 20), main_num, 1);

		printf("number %d calculation lap time: %.2fs\n", cal_num, time_fun(start_lap_time));
		printf("number %d calculation elapsed time: %.2fs\n\n", cal_num, time_fun(start_true_time));

		free(main_num);
		main_num = main_num2;
	}
	dig_red(&main_num);

	unsigned long long main_dig = dig_char_ll(main_num);
	print_list_num_des(main_num, my_min(66, main_dig));
	print_list_num_last(main_num, (long long)my_min(10, main_dig));
	printf("dig: %lld\n", main_dig);

	free(test_lst2);

	lst_check(main_num);

	if(EXE_PRINT){
		output_file(main_num);
	}
	
	free(main_num);

	printf("\nall calculation time: %.2fs\n", time_fun(start_true_time));
	printf("program termination\n\n");

	return 0;
}
