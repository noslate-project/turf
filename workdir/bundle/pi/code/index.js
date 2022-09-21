var pai = 0, flag = false;
for(var i=1; i<1000000000; i+=2) {
	pai += ((flag = !flag) ? 1 : -1) * 1 / i;
}
console.log('圆周率π：'+pai*4);
