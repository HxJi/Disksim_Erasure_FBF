/*
 * crs.h
 *
 *  Created on: Sep 17, 2014
 *      Author: zyz915
 */

#ifndef CRS_H_
#define CRS_H_

int cauchy_setup_tables(int w);

int cauchy_mult(int a, int b);

int* cauchy_good_general_coding_matrix(int k, int m, int w);

#endif /* CRS_H_ */
